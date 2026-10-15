# GR-OS change record

## 2026-10-15 — v0.6 Layer 3 maturation

- Preserved the unchanged 35-test v0.5 baseline, installed SDK, source archive and benchmark executables. Recorded assessment and ADR-0006 before implementation; retained the layered design after distinguishing runtime abstractions from repeated validation/access costs.
- Added bounded library-local weak-owner validation certificates and per-expression successful specialization reuse. Every call still checks logical/provenance/carrier constraints; new owners and failed validation are not trusted by digest.
- Added owning rank-one contiguous/strided access and endian-safe fixed-width loads; updated standard/sparse loops without changing their operation identities or checked arithmetic. Added bounded qualified-history sessions retaining validated parsed source state while preserving v1 bytes, new-owner checks, source revisions and selection losses.
- Added an optional numerical module with checked int64/uint64, finite binary64/complex128, explicit missing-first ordered reductions, no implicit promotion and scalar/batch providers. Floating execution initially requires the tested MSVC x64 strict profile; unsupported builds retain storage and integer computation. Approximate execution and reassociation remain deferred.
- Added immutable multi-output Function composition/binding and native/Python ownership tests. Extended explicit NumPy copy-in/read-only view-out to uint64/complex data without adopting NumPy promotion or nullable semantics implicitly.
- Added the independently configured/installed `gr_diagonal` source package with custom type/representation/operator, runtime composition, exact SDK dependency, relocated C++ consumer and separately installed Python C-facade wheel. Native extensions remain trusted; no portable plugin ABI or shared Python-handle registration is claimed.
- Added access/language/relational profiling, matched preserved-baseline measurements, independent extension performance coverage, conformance/fault/lifetime checks and review documentation. Final outcomes are recorded in [verification](docs/verification.md) and the [v0.6 review](docs/reviews/v0.6-review.md).
- Final full Release passed 38/38 in 106.64 s. ASan passed 36/37 initially; an installed-consumer missing-runtime-DLL setup failure was corrected and that case passed separately, covering all 37 selected tests. The affected Release case also passed again; unchanged legacy Python capacity remains excluded only from ASan. Actual NumPy passed in both configurations.
- Final paired 262,144-row table medians improved 854–933 to 23.5–24.1 ms; qualified 256-relation prepared transforms improved 983 to 114 ms. Small/cheap cache paths and several Python/NumPy import/export cases regressed, and qualified-history process memory increased; all samples and scope limits are retained in the benchmark report.
- All changes remain in GR-OS. Stop for architectural review after v0.6 verification; no broad Layer 2 implementation or legacy/application migration is included.

## 2026-10-14–15 — v0.5 Layer 3 computational language

- Preserved the unchanged 28-test v0.4 source/installed baseline and verification evidence before implementation; recorded architectural assessment and ADR-0005.
- Added versioned structural logical type families, separate representations, generic operator inference and concrete specialization with recursively pinned contracts. New mathematical structures register through extensions without kernel switches.
- Added immutable lazy expressions, explicit shared-subexpression reuse, named input templates, selected-root lowering and inspectable transformation recipes. Reused the existing planner, scheduler, provider selection, quotas and policy.
- Added nullable numerical/tabular processing, explicit missingness and checked integer arithmetic; a separately installed canonical CSR extension; and qualified historical incidence/count processing retaining source qualifications, pinned referents and explicit selection losses.
- Added native C/Python typed data/expression/program access over existing buffer, plan and job ownership, plus installed consumer and interoperability validation. Kept the v0.4 experimental consumer available.
- Expanded conformance, pilot, lifetime, historical and extension tests, benchmark evidence, architecture documentation and review agenda. Detailed outcomes and limitations are recorded in the v0.5 verification/review artifacts.
- Final Release passed 35/35 cases in 93.11 s; selected AddressSanitizer coverage passed 34/34 in 274.14 s. The unchanged legacy Python capacity suite was excluded only from this sanitizer run; optional NumPy passed in Release and was skipped in the isolated sanitizer environment. Generic table/history performance remains an explicit improvement priority.
- Stop for architectural review after v0.5. No legacy/application migration, Layer 2 scientific model or unrelated programme change is included.

## 2026-10-13 — v0.4 foundational refinement

- Preserved the v0.3 source hashes, installed SDK and matched benchmark inputs; recorded architectural assessment and ADR-0004 before substantive implementation.
- Added content-addressed checkpoint/delta history objects and incremental catalogues with bounded selected reconstruction, exact v1 semantic export, retained branches, explicit upgrade and atomic publication. Existing v1 append encoding remains available and stable.
- Added bounded transaction-local immutable validation reuse, optional process-local FIFO writer admission, cancellation/deadline-aware queueing, stale-head preflight preserving receipt replay, and phase diagnostics. Workspace CAS, WAL/FULL and frozen recovery remain mandatory.
- Removed redundant storage canonicalization and reused snapshot-membership SQLite statements without dropping integrity checks or changing schema 1.
- Added aligned owned host buffers, checked typed views/slices, allocation lifetime budgets, named buffer sets and numerical declarations pinned by operations/providers. Logical mathematical types remain separate from physical layout.
- Added additive C/Python buffer ownership and historical selection interfaces, optional genuine read-only CPython buffer export, and a separate experimental historical-data transformation/reduction module shared by C++ and Python.
- Extended conformance, concurrency, corruption, lifetime, numerical and installed-consumer tests; preserved process-crash recovery coverage for the new multi-object history publication. Added matched admission/history and writer-policy performance evidence.
- Recorded 28/28 passing Release cases, all 28 cases passing across corrected ASan runs, installed SDK verification and fresh v0.3 comparisons. The [verification report](docs/validation/verification-v0.4.md) and [measured analysis](docs/validation/benchmark-v0.4-analysis.md) preserve improvements, regressions, initial failures and qualification limits.
- Updated architecture, contributor guidance and roadmap. Stop after v0.4 review; no broad Layer 3, legacy migration or programme source changes.

## 2026-10-13 — v0.3 GR Systems Runtime

- Recorded a comprehensive Layer 4 assessment before implementation and ADR-0003 for prepared execution, scheduling/resource ownership and native atomic semantic publication.
- Added sealed module/backend catalogs, exact capability-based provider selection, typed whole-DAG validation, immutable prepared plans and bounded caching; added an actual L5 record-verification operation.
- Added shared immutable operand payloads and prepared L5/L6E handles to avoid repeated descriptor preparation and graph-edge byte copying. The experimental native Value source API changes deliberately; C ABI 1 remains additive.
- Added a fixed native worker pool, dependency/control edges, priority aging, serial mode, bounded admission/result retention, cancellation, cooperative deadlines, joined failure/shutdown, explicit operation/workspace policy and bounded traces.
- Moved semantic admission from C binding composition into a public native transaction coordinator with pinned bases, bounded staging, whole-workspace CAS, rollback, uncertain outcomes, receipt recovery and frozen exact retries.
- Added atomic custody of computed artifacts, optional semantic-history bundles and a technical manifest, preserving linear physical history and distinct semantic ancestry without changing SQLite schema 1.
- Added owned plans/jobs/transactions to C/Python, synchronous and asynchronous Python access, a standalone machine-readable native CLI, installed runtime consumers and an integrated computed-artifact/history example.
- Added concurrency/failure/stress and genuine process-termination/restart tests, native dispatch/scaling benchmarks and concurrent WAL/FULL transaction measurements. Verification records actual results and qualification limits.
- Updated contributor guidance, architectural contracts, compatibility/readiness decisions and deferred work. All changes remain under GR-OS; no Layer 1–3 implementation, legacy migration or programme publication changes.

## 2026-10-13 — v0.2 foundational reconstruction

- Recorded an architectural assessment before reconstruction and ADR-0002 for independent semantic/physical contracts, historical scope and compatibility decisions.
- Reorganized cohesive support, kernel, storage, execution and C/Python binding components with public dependency boundaries and compatibility umbrella headers.
- Added versioned operand descriptions, pinned typed operation contracts and independent physical provider registration; multi-input/result calls and multiple exact CPU implementations now share one meaning.
- Added immutable record values, consistent profile-aware projections, declared event/order capabilities and accurate staged revision provenance. Kept original canonical record bytes and readable opaque v1 identifiers.
- Added pure recording-history DAG checkpoints, reconstruction/differences, explicit-base conflict handling and required-semantics checks. Physical storage remains a separately declared linear ledger.
- Added typed storage guarantees, receipt lookup and independent verified range readers, including cancellation and pinned lifetime after store close. Strengthened pre-copy limits and ledger integrity without changing SQLite schema 1.
- Added strict duplicate-key metadata parsing, binary C/Python reads, binding resource ownership/cleanup, input-budget checks and the 10000-current-record replacement fix.
- Added relocatable CMake installation, separate Python wheel packaging, installed consumer validation, conformance/model/failure tests, synthetic extension examples and reproducible before/after benchmarks.
- Documented public ownership/error contracts, receipt integrity limits, checkpoint costs and unimplemented runtime/device/migration boundaries. All reconstruction remains under GR-OS; stop for review after this increment.

## 2026-10-13 — Layers 5 and 6 implementation

- Recorded the user's selection of explicit layered development, C++/Python direction, and implementation of Layers 5–6 followed by a stop for review in ADR-0001.
- Added the native semantic kernel, bounded canonical metadata, separate record/extension registries, whole-candidate validation, retained references, n-ary incidence projection, equality matching and selected-account temporal cuts.
- Added interchangeable volatile and SQLite storage with atomic record/artifact/receipt publication, expected-head concurrency, exact transaction retries, immutable versions, pinned snapshots, integrity checking and paged history.
- Added host CPU discovery and explicitly selected sequential/threaded execution with versioned trusted callbacks, bounded batches, stable outputs, cooperative cancellation/deadlines and failure propagation.
- Added a versioned C ABI, Python `ctypes` access, a runnable temporary-database example, native/integration tests, build helper, and pinned dependency provenance/licenses.
- Added source and candidate protection for unknown required semantics, separate extension-validator registration, diagnostics for unsupported temporal interpretation, and indeterminate interval handling following independent code review.
- Documented supported contracts, verification evidence and concrete limitations. No Layer 1–4 runtime/application, legacy migration or changes to existing programme/infrastructure source.

## 2026-10-13 — Initial assessment

- Created the separate reconstruction workspace at the user's request.
- Added contributor guidance, an architectural assessment, and open decisions for discussion.
- Reviewed the R8 handoff against live local source, tests, specifications, decisions, and relevant foundation passages.
- No implementation or data migration started. No architecture recommendation marked accepted. Existing programme and infrastructure source left unchanged.
