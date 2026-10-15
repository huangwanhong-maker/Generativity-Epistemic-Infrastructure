# Scheduler, resource and job contracts

**Class:** Implementation specification  
**Status:** EXPERIMENTAL, GR-OS v0.4

`gr::runtime::Runtime` owns a fixed pool, shared scheduling state and one deadline
monitor. Default capacity is the smaller of four and reported logical CPUs.
Capacity is per runtime instance. The public API is in
[`scheduler.hpp`](../include/gr/runtime/scheduler.hpp).

## Admission and ownership

`submit` accepts an immutable `PreparedPlan`, job options and optional publication.
It checks pure-operation policy, required node CPU slots, literal bytes, limits and
publication permission before making the job runnable. Admission reserves one job
slot and the requested retained-payload cap; publication additionally reserves its
staging cap. Private transaction begin/staging happen without the scheduler mutex.
The base is captured then, before computation. Unsupported capability/authority
requirements do not start provider callbacks.

When publication requests serialized local writer admission, transaction begin can
wait before a runnable job exists. The job deadline covers that queue; runtime
shutdown also signals pending admission. The writer lease is process-local and
does not hold an open SQLite transaction during computation. Physical workspace
CAS remains mandatory, including for external writers and explicit expected bases.

Defaults are 64 live jobs and 256 MiB total logical reservations; job payload is
64 MiB by default. Retained literals and every completed node's output count, even
if results share immutable storage or are no longer needed by later nodes. The
conservative sum is simple and inspectable; last-use reclamation is deferred.
Per-node input/output limits and aggregate retained-output admission are checked.
Excess output fails the job without publishing partial results.

Buffer values use the maximum of logical element bytes and distinct retained owner
bytes within each value, plus bounded layout metadata. Cross-value aliases may be
charged repeatedly. This prevents a tiny slice or a broadcast view bypassing the
cap. A separate optional `AllocationBudget` follows the native allocation through
its final owner, including aliases extracted from results. See [buffers](buffers.md).

This is a retained logical payload budget. In-flight callbacks can simultaneously
produce temporary outputs before aggregate acceptance; those temporary allocations
are outside that cap. Validators, JSON/container metadata, plans, transaction base
snapshots, storage copies and native/Python caller copies also have distinct costs.
Arbitrary trusted callbacks can allocate independently. There is no hard allocator,
RSS or OOM-survival guarantee. Completion bookkeeping has a containment path to
repair slots and report a failure if diagnostic allocation itself throws, but
global allocation exhaustion is not a fully qualified failure model.

`Job` is copyable owning control/result access. Native `JobResult` exports retain
the same admission lease. Capacity returns after all such leases and internal
worker references are released; the final internal cleanup may lag completion
notification briefly. Copying a `Value` out of its result transfers responsibility
for that alias to the caller and does not keep the runtime reservation alive.
Python byte/JSON results are independent copied values; buffer results own native
view handles. The native `Job` handle owns its runtime lease until closed. Buffer
allocation budgets continue to follow their owners independently. Releasing a live
job handle does not assert cancellation.

## Scheduling and ordering

Nodes become ready only when every data/control predecessor has completed.
Ready order within a job is stable by original node index. A single mutex guards
queues, dependency counters, slot ownership and state transitions. Provider calls,
storage I/O and foreign stop callbacks run outside it. The current queue scan is
linear in live jobs; this avoids an unverified lock-free/work-stealing subsystem.

Priority is -16 through 16. Dispatch-count age increases while a ready job waits;
its effective score is priority plus age. High-priority arrivals cannot bypass a
continuously ready low-priority job indefinitely if callbacks return and suitable
capacity becomes available. This is not a latency SLA or preemptive scheduler.
Tasks requiring several CPU slots can wait for sufficient free capacity; there
is no strict reservation fairness guarantee for large-slot tasks under saturation.

`serial=true` allows one active node of that job at a time, preserving stable ready
order. Other jobs may still run concurrently. Exact result semantics come from
the declared providers; elapsed timings, inter-job order and the first observed
concurrent failure are not deterministic. Current prepared CPU providers need one
slot; a future internally parallel backend must honor its declared slot lease.
Backend prepared dispatch does not create a nested CPU batch scheduler.

## Failure, cancellation and deadlines

An operation exception stops unscheduled descendants and requests cooperative
cancellation of active siblings. The job becomes terminal only after siblings
return. A queued idle job can be cancelled while all workers are occupied. The
deadline monitor similarly retires expired idle jobs and signals running calls.
Per-job deadlines start at admission and cannot exceed one day. Poll/wait timeout
only limits that caller's waiting; it is distinct from job cancellation/deadline.

A provider that ignores its stop token may delay cancellation and shutdown.
The runtime does not kill threads, unwind foreign callbacks or undo arbitrary
provider effects. Executable DAG operations currently require pure exact contracts.
Nested submission, synchronous waiting, or shutdown on the same runtime from its
own worker is rejected to avoid pool self-deadlock. Destroy the runtime outside
its own provider callbacks.

`shutdown` rejects new admission, interrupts queued serialized admission, requests cancellation and joins the worker pool
and deadline monitor. Concurrent external shutdown calls serialize. Results remain
inspectable through retained job leases afterward. As with normal C++ objects,
destroying a runtime while another thread accesses that object is caller misuse;
own its lifetime explicitly.

## Publication and recovery

After every node succeeds, one worker crosses the publication barrier. Declared
exported byte values can populate attributed artifact records. Equal artifact
digests are uploaded once, including values already present in initial staging.
L5 validation and the native transaction coordinator own final semantic admission.
No database transaction remains open while nodes execute.

Cancellation before publication prevents a commit. If storage acknowledges a
commit, the job succeeds even when cancellation arrives late. An unacknowledged
publication can produce terminal `outcome_unknown`, with no claim of rollback.
`recover_publication(false)` checks the receipt; `true` retries the exact frozen
request. Neither repeats graph execution. Successful recovery changes the job to
`succeeded`. Native synchronous `run` preserves a `JobResult.recovery` job lease
when the outcome is uncertain, so its frozen request and computed values remain
available. Release it deliberately after recovery. Python provides the corresponding
recovery job on an uncertain convenience result. Read [transaction guarantees](runtime-transactions.md) before treating
job status or an absent receipt as evidence of persistence.

## Diagnostics and verification

Job diagnostics include plan identity, serial mode, priority, reservation and peak
retained bytes, bounded event trace and dropped-event count. Runtime diagnostics
include worker/control-thread counts, current/peak slots, live jobs/reservations,
submitted/terminal jobs and completed instructions. Traces include provider/backend
selection, node starts/completions/failures, cancellation and publication events.
They are local operational observations, not durable or authenticated provenance.

Unit tests exercise dependency order, true callback overlap, serial execution,
joined failure, quota leases, queued/running cancellation, deadlines, fairness,
concurrent submitters, shutdown, policy denial and computed-artifact publication.
Native prepared and runtime benchmarks separate dispatch from scheduling costs.
See [verification](verification.md) for results and qualification limits.
