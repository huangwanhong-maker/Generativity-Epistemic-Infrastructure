# Runtime transaction contract

Publication class: implementation specification. Status: EXPERIMENTAL, GR-OS
v0.4. This is a bounded local runtime contract, not an adopted GR Standard.

`gr::runtime::TransactionCoordinator` is the native semantic publication path.
It composes L5 record/history validation with an L6 transactional store; the
storage adapter itself continues to accept opaque records. The C and Python
interfaces use this coordinator rather than duplicating admission rules.

## Scope and guarantees

The atomic unit is one workspace in one already-selected backend. A successful
publication includes its record versions, complete current snapshot, supplied
binary objects, referenced object inventory, optional semantic-history artifact
and manifest, transaction receipt, and head movement in one L6 commit.

| Property | Concrete guarantee and boundary |
| --- | --- |
| Atomicity | MemoryStore swaps a complete accepted state; SQLiteStore publishes all components in one SQLite transaction. Local computation and external effects are outside this transaction. |
| Consistency | Admission checks the source and complete candidate against registered L5 interpretations, direct pinned references, content identities, artifact lengths, and required artifact custody. Hashes establish byte identity, not truth, authorship, authority, or signatures. |
| Isolation | `begin` pins a complete workspace revision. A mutating publication requires the whole workspace head still to equal that revision. Concurrent accepted writes therefore have a workspace order; even unrelated concurrent edits conflict. Read-only transactions retain their original view. |
| Durability | MemoryStore is volatile. SQLiteStore uses WAL and FULL synchronization; persistence depends on the local filesystem and device honoring synchronization. Tests establish process-termination/restart behavior, not arbitrary power-loss survival. |

`Isolation::workspace_snapshot_cas` is the only implemented isolation mode.
Unsupported isolation or storage requirements fail before computation. There is
no silent downgrade, automatic rebase, nested transaction, savepoint, distributed
transaction, cross-store two-phase commit, or external-effect exactly-once claim.
No SQL transaction stays open between `begin`, caller computation, and `stage`.

## Native lifecycle

1. Open a store, explicitly create a workspace, and construct a coordinator.
2. Call `begin(TransactionOptions)` with workspace, stable transaction identity,
   actor, reason, and optionally an exact base revision. An empty base captures
   the current snapshot revision. `options()` returns the captured options.
3. `stage(Stage)` accepts complete L5 record wrappers and bounded complete binary
   uploads. Each record identity and supplied blob digest can be staged once;
   dependent records can arrive in separate stages. Staging validates individual
   envelopes, while `snapshot()` and `commit()` require a coherent whole candidate.
4. `commit` checks admission, freezes the exact physical request and its digest,
   then invokes the backend once. Cancellation/deadline checks before publication
   leave no effect. A successfully acknowledged backend commit wins cancellation
   that arrives during or after publication.
5. `rollback` discards unpublished local work. It cannot undo an accepted commit,
   resolve an uncertain outcome, or roll back application effects.

Read-only transactions disallow staging and complete without a physical receipt.
Unknown optional record interpretations remain opaque and preserved. A required
unknown interpretation remains readable but blocks mutation of the source, even
if an update attempts to replace that record with a known type.

Objects are thread-safe through a transaction mutex; methods are synchronous and
state reads wait while a commit is executing. Registries are copied at coordinator
construction, with trusted pure validators expected to remain stable. The scheduler
owns a private publication transaction for each publishing job; callers do not
concurrently stage into that transaction. Releasing/destroying an uncommitted
transaction releases local staging memory and publishes nothing.

## Recovery and uncertain outcomes

`outcome_unknown` means the backend call did not return an acknowledged outcome.
It is deliberately conservative for adapter failures whose publication point is
not known to the coordinator. A quota error after entering the adapter can thus
be reported as uncertain even when a particular adapter has rolled back internally.
The original cause code is retained in error details.

`recover()` looks up the stable transaction identity and verifies its exact request
digest and actor. A matching receipt yields the accepted revision and the current
workspace head separately. An absent receipt yields `not_observed`: it describes
that read, and does not rule out a concurrent later publication. An already
acknowledged receipt becoming absent is an integrity error.
Later backend unavailability during a retry does not erase an already acknowledged
`committed` state; that retry reports its backend error while retaining the known result.

`retry()` resubmits the frozen physical request. It does not reinterpret records,
recompute output, choose a new base, or rerun semantic validators. L6 checks replay
identity before stale-head conflict, so an accepted request can be recovered after
other transactions advance the workspace. A reused identity with different
content is rejected.

After process restart, the coordinator exposes receipt lookup. The caller can
also recreate the exact original request using the original base and transaction
identity; this fresh transaction performs admission again. Unlike a live frozen
retry, reconstruction across changed validators or runtime contract versions is
not guaranteed to reproduce a request. Durable serialization of logical staging
and version-spanning validator replay are not implemented.

`TransactionResult.history` describes the prepared history intent until the state
is `committed`; it is not independently proof of publication. A physical receipt
is authoritative for acceptance. A conflict requires deliberate reconciliation
and a new transaction identity, rather than retrying changed content under the
old identity.

## Atomic semantic-history publication

Storage revision identity and L5 semantic-history node identity remain distinct.
SQLite retains a linear physical publication ledger. A history bundle can contain
branches or explicit merges without changing that storage history model.

New automatic `track_history=true` publication uses compact immutable history
objects and a reserved `gr:runtime:history` record with profile
`gr.runtime.history-manifest@2`. Its catalogue and selected semantic head are
separate physical and semantic identities. The runtime excludes this control
record from user snapshots and rejects user staging of its identity. A caller
registry cannot shadow either reserved profile.

Existing profile-1 manifests remain readable and retain their exact bundle append
encoding unless `upgrade_history=true` is explicitly selected. Fresh explicit v1
imports also retain profile 1 unless upgrade is requested. Upgrading is bounded
and atomic; compact physical request/revision identities deliberately differ,
while each existing semantic node identity and reconstructed v1 state is preserved.
The standalone `HistoryStore::upgrade_v1` helper preserves the selected head.
An ordinary coordinator commit with `upgrade_history=true` converts the existing
nodes and appends its normal attributed commit node, so its exported bundle grows.
See [history storage](history-storage.md).

Once a manifest exists, later ordinary mutations automatically append history,
even if `track_history` is false. Each generated checkpoint records the actor,
method, reason, transaction identity, and physical base revision. Enabling history
does not reconstruct physical revisions that preceded enablement.

`stage_history(HistoryPublication)` imports an explicitly selected bundle/head.
Its selected current record inventory must exactly match the candidate's IDs and
content versions; selected required interpretations must be available. Every
previously retained semantic node must remain byte-equivalent in the new bundle.
All record and archived-record artifacts in every node must be available in the
same workspace or supplied in the publication, including unselected branches.
The chosen checkpoint, complete artifact custody, manifest, physical snapshot and
receipt are accepted atomically. An imported branch can select a different
semantic parent while still publishing after the current physical head.

The manifest is a runtime technical profile, not a new GR record-ontology class.
Raw L6 callers can bypass semantic policy; applications that need semantic
admission should use the coordinator. The runtime detects a mismatched manifest
or checkpoint rather than repairing it silently.

## Bounds and costs

Staging defaults to 64 MiB and permits an explicit budget of 1 byte through
128 MiB. The budget covers serialized staged record wrappers, explicit required
digest strings, uploaded binary bytes, and a supplied v1 history bundle or generated history objects.
An explicit history bundle is charged once. It is a logical payload bound, not an
exact resident-memory or allocator limit: native JSON values, validation indexes,
copies and backend state have additional bounded costs.

The current adapters permit at most 1,000 upserts per commit, 10,000 current
records, 128 supplied objects, 16 MiB per object by default, 64 MiB supplied
objects per commit, and 100 MiB retained objects per workspace by default.
Each history object is capped at 16 MiB. A technical manifest consumes one
upsert/current slot. Ordinary compact append normally creates a state payload,
small node header and catalogue object; import/upgrade must fit the same 128-object
atomic limit. Legacy explicit bundles remain available when a conversion would
exceed that bound.

Semantic history remains bounded to 1,024 nodes. Reconstruction/read budgets can
reject a full export before that ceiling; the node bound is not a capacity promise.

Compact histories retain periodic full checkpoints and smaller changes between
them. Selected reconstruction follows a bounded physical chain; semantic parents
remain independent of compression bases. Catalogue lookup of an arbitrary old
revision is linear in catalogue history; full export/audit checks the complete
DAG. Current-head reads do not audit every disconnected branch. No deletion,
garbage collection or automatic archival policy is introduced. Full state-inventory
hashing, candidate reference checks and physical snapshot membership still cost
O(snapshot); this is not a general-purpose scalable database engine.

The coordinator's `history_snapshot` first loads the pinned physical snapshot and
reconstructs its manifest's current head to check their agreement. Selecting a
different semantic revision then reconstructs that state, so one request may
visit two reconstruction chains. A corrupt current head can prevent this API
from selecting an otherwise intact older node. Standalone selected reconstruction
has a narrower verification boundary; these are intentionally distinct APIs.

## Admission reuse and writer policy

Each transaction owns a `core::ValidationSession` with a copied registry. It memoizes
valid complete wrappers only after verification and compares exact wrapper values
before reuse; retaining the same claimed digest with changed bytes cannot use a
memo entry. Source required interpretations and candidate-wide references, temporal
anchors and participants are always checked. The memo is bounded to 20,000 wrappers
and 64 MiB of canonical document payload. This is not a process-memory quota.
Trusted callbacks must remain pure and stable throughout the transaction.

`AdmissionPolicy::optimistic` remains default. `serialized` explicitly queues
cooperating writers before capturing the current base and requires an empty
`expected_revision`. FIFO leases are process-local, workspace-scoped and
non-thread-affine. Memory/custom adapters coordinate the same store object unless
they provide a coordination identity; SQLite uses its resolved filename as a hint.
Different aliases/processes and direct storage writers can still race, so physical
CAS remains mandatory. Queue waits support cancellation/deadlines; callers should
bound long computation/staging lifetimes because a held lease delays other writers.

The lease holds no SQL transaction or store mutex across user computation, so
readers continue using snapshots. Publication attempts, rollback and destruction
release it. Unknown outcomes retain the exact frozen request after lease release.
An early stale-head check avoids expensive preparation only when no receipt for
that identity exists. Such a rejection is a definite `conflicted` result with no
frozen request. A prior receipt still follows exact preparation/comparison, allowing
accepted replay after later publications. No policy silently rebases or reexecutes.

`TransactionResult.diagnostics` reports begin/admission-wait, candidate admission,
publication preparation and physical commit durations, plus validation/history
counts. These timings include bookkeeping and may overlap nested storage work;
they are observations, not an SLA or CPU profiler.

## Validation evidence and limits

`tests/conformance/admission.cpp` additionally checks immutable memo reuse and
forgery resistance, copied interpretation, queued cancellation/deadlines, cross-thread
lease release, concurrent readers, serialized dependent updates over independent
SQLite connections, external-policy races and replay after stale preflight.

`tests/conformance/transactions.cpp` runs the same lifecycle, semantic-history,
CAS, and resource cases against MemoryStore and SQLiteStore. It also checks
mandatory unknown interpretations, reserved identity handling, exact legacy
publication identity, explicit-history byte accounting, missing historical
material, genuine concurrent coordinators, quota exhaustion, cancellation,
acknowledgment loss, receipt recovery, and a frozen retry after a validator changes.
`tests/unit/runtime_recovery.cpp` verifies that synchronous uncertain job results
retain a recovery handle after the runtime closes, and that receipt recovery or
frozen retry restores original computed outputs without rerunning their provider.

`tests/integration/runtime_crash.py` starts a native worker in a separate process.
A test-only SQLite trigger and registered function create a deterministic barrier
inside the actual publication transaction, immediately before the head update.
The parent forcibly kills the worker before publication, at that internal barrier,
and after an accepted commit before acknowledgment. Fresh processes run SQLite
integrity checks, inspect records/objects/receipts/history, and perform exact
idempotent retries. The trigger exists only in isolated test databases; production
storage has no crash hooks. These tests do not simulate torn sectors, controller
cache loss, remote filesystems, machine power loss, or malicious database editing.

The current schema preserves its v1 identity format. Its previously documented
attribution-field tamper-detection limitation remains; this change does not claim
a signed or fully tamper-proof ledger. See [storage contracts](storage-contracts.md)
and the recorded build/test evidence for actual platform coverage.
