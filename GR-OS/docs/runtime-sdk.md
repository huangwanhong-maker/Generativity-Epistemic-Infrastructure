# Runtime SDK and command interface

Classification: experimental implementation documentation, GR-OS 0.5. This is an
embedded local runtime and an executable command interface. It does not implement
a daemon, network protocol, sandbox, authenticated identity, or Layers 1ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ3.

## Shared native implementation

Installed C++ consumers use `find_package(GR 0.5 REQUIRED COMPONENTS runtime)` and
link `GR::runtime`. Public headers are under `gr/runtime/`; `gr/runtime.hpp` is the
umbrella. The versioned C ABI remains ABI 1 and adds runtime, plan, job, and
transaction operations. The Python wheel is a transport facade over that same
library. No Python semantic implementation or callback thread is involved.

Python `Runtime` and the C runtime context own a sealed catalog and one fixed
scheduler pool. C++ exposes catalog, planner, and runtime as separate owned
components so applications can install trusted extensions before sealing. Plans retain pinned
operation/type/provider/backend contracts. A successful `plan` call performs
whole-graph admission before submission; selection diagnostics are available in
`Plan.description`. Every declared node runs, including nodes omitted from the
selected output list. An `after` edge orders execution, independently of a typed
data dependency. This ordering does not assert occurrence order in a GR record.

```python
from gr_os import Runtime, Graph, Node, OutputRef, Value

with Runtime(workers=2) as runtime:
    graph = Graph([
        Node("copy", "gr.bytes.identity@1", [Value.bytes(b"hello")]),
        Node("hash", "gr.bytes.sha256@1", [OutputRef("copy")]),
    ], [OutputRef("hash")])
    with runtime.plan(graph) as plan, runtime.submit(plan) as job:
        result = job.result(timeout=5).raise_for_status()
        print(result.outputs[0].data.decode("ascii"))
```

Python requires an explicit `Native(path)` or `GR_OS_LIBRARY`; runtime features
require 0.3 or later; owned buffers/history selection require 0.4. `Native` may be shared across threads. Pass that same
instance to participating `Runtime`, `Storage`, and `Buffer` objects. JSON/byte
literals cross the JSON/base64 boundary by value; BufferSet literals transfer
validated immutable native handle ownership. Later Python mutation does not change
an admitted plan. Python buffer imports copy once into native ownership; optional
readonly exports expose that native allocation without another payload copy.

## Jobs, ownership, and cancellation

`submit` returns an owning `Job` lease. `status` is a snapshot of native job state.
`wait(timeout)` returns a structured `JobResult` or `None` when the caller's wait
expires. `result(timeout)` raises Python `TimeoutError` only for this wait expiry.
Terminal failed/cancelled/deadline/outcome-unknown states remain structured results;
call `raise_for_status()` when an exception is appropriate. Publication receipts
and errors remain attached to that exception's details.

`timeout_ms` at submission defines the native job deadline from admission.
`cancel()` requests cooperative cancellation. It cannot forcibly stop an arbitrary
trusted native callback and cannot undo a committed publication. Python `await
job` polls native status without blocking on a long native wait. Cancelling that
await requests native cancellation and propagates `asyncio.CancelledError`; it
does not certify native termination. Synchronous waits poll at most 100 ms per
native wait so Python can process interrupts between calls.

An uncertain synchronous `Runtime.run` result retains `result.recovery`, an owned
Job for receipt inspection or exact frozen retry. Use `result.recover_publication()`
and close the result/recovery lease explicitly when finished. The C synchronous
calls return `recovery_handle` in this case, usable with `job.recover`/`job.close`
inside the same embedding process or CLI session. Ordinary asynchronous `job.wait`
does not create another handle: the caller already owns the original Job.
Recovery handles do not survive process exit; persistent receipts and original
request identity remain necessary for restart recovery.
`raise_for_status()` retains the result as `exception.result`, including its
recovery lease; close it after handling an uncertain outcome. Result objects are
snapshots: after recovery, query `result.recovery.result()` for the updated job.

Close plans and jobs explicitly or with context managers. Completed native job
results still consume live-job and payload admission capacity until their last
native lease is released. Python-decoded JSON/byte copies belong to the caller
and are outside this native retention budget. Buffer outputs own separate native
handles; close their Value/BufferSet or JobResult explicitly. An exported memoryview
retains its allocation independently after that close. `Job.close()` releases its lease; it is
not a cancellation command. `Runtime.close()` removes the handle, requests stop,
and joins workers outside the global handle registry mutex. Job result/control
leases remain readable after runtime close. A call racing before native handle
acquisition can receive `invalid_handle`; calls that acquired a native lease
retain the object safely.

Worker completion bookkeeping may briefly retain a lease after the terminal state
is visible; backpressure can therefore persist until that bookkeeping finishes.

Runtime finalization is a fallback and can block while joining a native callback.
Use explicit close; do not unload the native library while resources exist.

## Transactions and publication

`Transaction(store, workspace, expected_revision=..., transaction_id=..., actor=...,
reason=...)` freezes a base snapshot while retaining the native store. Stage record
wrappers/blobs and optionally an explicit semantic history bundle, then commit.
The transaction exposes `status`, `snapshot`, `rollback`, `recover`, and `retry`.
No database transaction is held open during user computation. Rollback discards
unpublished staging; CAS conflicts are not silently rebased. An uncertain outcome
is recovered against the frozen request identity, not replaced with a fresh ID.
Attribution strings are declarations, not authenticated principals.

`Publication` submits private transaction options, initial staging, and output
artifact bindings with a job. The runtime policy must explicitly allow the
workspace. The native scheduler publishes only after all nodes succeed. Failed
computation produces no successful transaction receipt. The publication binds
computed bytes to record artifact descriptors and validates the complete candidate
through the single native transaction coordinator. Existing `Storage.commit`
delegates to that coordinator too. See [the complete executable example](../examples/python/runtime.py).

Storage requirements are negotiated explicitly, including persistence, atomic
publication, snapshot consistency, conditional commits, and blob-reader features.
Memory cannot satisfy persistence. Local SQLite publication atomically commits
records, blobs, receipt, and head; this does not claim atomicity across unrelated
stores, network services, or external effects. `track_history=True` retains the
semantic DAG using a technical manifest, separate from the physical linear ledger.

## C ABI operations and wire graph

Requests retain `{ "abi": 1, "op": "...", ... }`; responses retain the success/error
envelope and allocated response ownership described in [sdk.md](sdk.md). Added
operation families:

| Family | Operations |
| --- | --- |
| `runtime` | `open`, `close`, `capabilities`, `plan`, `submit`, `run`, `evaluate` |
| `plan` | `describe`, `close` |
| `job` | `status`, `wait`, `result`, `cancel`, `recover`, `close` |
| `transaction` | `begin`, `status`, `snapshot`, `stage`, `stage_history`, `commit`, `retry`, `recover`, `rollback`, `close` |

`runtime.open` takes `config` and returns `handle` plus `catalog`. `runtime.plan`
takes that `handle` and `graph`, returning `handle` for the plan and `plan`
diagnostics. `runtime.submit` takes runtime `handle`, `plan` handle, optional
`options` and `publication`. `job.wait` takes a job `handle` and `timeout_ms`
within 0ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã…â€œ60000, returning `{ready,status,result?}`. `runtime.evaluate` creates,
runs, and drains a private runtime for a single graph. Handles are process-local;
they are neither durable identities nor security capabilities.

```json
{"nodes":[{"id":"copy","operation":"gr.bytes.identity@1","inputs":[{"literal":{"type":"gr.bytes@1","representation":"bytes","data_base64":"aGk="}}]}],"outputs":[{"node":"copy","output":0}]}
```

Operands contain exactly one `literal` typed value or `ref:{node,output}`.
Optional node fields: `after`, `selection:{implementation,backend,device,
prefer_implementations}`, `max_input_bytes`, `max_output_bytes`. Explicit selection
failures do not silently fall back. Graph and configuration unknown keys fail.
The transport bounds remain 16 MiB/request, 64 MiB/response, depth 64. A typed JSON
value remains bounded by the 1 MiB canonical contract; larger store snapshots do
not implicitly become valid typed JSON operands. JSON transfer can fail after an
accepted commit; retain the transaction ID and inspect a receipt/recover before
deciding whether to retry. Binary readers retain their separate raw C ABI.

## Standalone native CLI

The installed `gr` executable requires the installed native library and no Python.
It emits only one JSON response per command/request to stdout. `gr help` documents
the command/exit-code contract. Input is bounded before parsing.

- `gr capabilities`: SDK feature/version envelope.
- `gr call [--file PATH]`: one ABI request from stdin or an explicit file.
- `gr run [--file PATH]`: one object containing `graph` and optional runtime
  `config`/job `options`; creates a runtime, executes, drains, returns JobResult.
- `gr session`: bounded JSONL ABI requests/responses with handles retained inside
  that process. Close resources explicitly. EOF ends the process and joins runtimes.

Exit codes: 0 successful command/session, 1 native request error, 2 input/usage/
transport error, 3 terminal job failure for `run`. A session continues after native
request errors and returns a nonzero final code if any request failed. There is no
one-shot detached submit command or hidden server. Use an embedding process or an
explicit session for asynchronous jobs. No current-directory config, credentials,
or external module paths are automatically discovered.


## Historical selection and writer admission (v0.4)

`Storage.history_snapshot(workspace, semantic_revision="", physical_revision="")`
returns the selected reconstructed snapshot with explicit `.provenance` and
`.history_diagnostics`. An omitted physical revision is captured before manifest
selection. Immutable artifact digests remain workspace-qualified.
`history_export_v1` explicitly materializes the bounded legacy bundle when needed;
ordinary selected reconstruction does not require exporting the entire DAG.

Transaction options accept `admission="optimistic"` (default) or `"serialized"`.
Serialized admission requires `expected_revision=""`; it queues before selecting
the latest base. The optional begin `deadline_ms` and initial `cancelled` flag bound
queue admission. Native C++ additionally supports live stop tokens. This process-local
queue does not replace the store's conditional commit check against other writers.
`upgrade_history=True` explicitly requests atomic legacy-to-compact representation
upgrade. See [transaction guarantees](runtime-transactions.md) for replay and
uncertain-outcome limitations; no SDK convenience changes those guarantees.

The optional [historical data experiment](../experiments/data/README.md) calls the
same C++ selection/typed transform/reduction implementation from C++ and Python.
`Runtime(modules=["gr.experimental.data@1"])` opts into its catalog descriptors.
Its owned BufferSet results and exported memoryviews survive job/runtime closure;
close result handles explicitly. Native runtime diagnostics remain operational
observations, distinct from the recipe's selected source provenance.

Version 0.5 adds the [typed data SDK](data-sdk.md). Its compiled programs expose
ordinary native plans and submit through this same scheduler and transaction path.
`DataJob` retains both the program and job; materialization checks the actual native
result and pinned recipe context. An optional graph `context` object participates
in plan identity when nonempty; absent/empty context preserves the previous graph
identity. Context metadata does not itself grant runtime authority.

