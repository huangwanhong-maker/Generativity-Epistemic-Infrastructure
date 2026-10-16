# GR-OS

**Class:** Experimental implementation workspace  
**Version:** 0.7.0 — Layer 2 tools and independent module packages

GR-OS provides C++20 and Python tools over a shared computational framework, systems runtime, relational-historical kernel and storage/execution backends. v0.7 adds independent qualified analysis and visualization packages, conventional module discovery, and composition with the existing diagonal mathematical extension. Native tools share immutable views; Python packages exchange explicitly copied projections. v0.7 is complete and stopped for architectural review. Release passed 40/40 tests and the selected ASan suite passed 39/39, with targeted final Python and large-fixture corrections also verified. See the verification and performance reports for exclusions, regressions and limits.

| Component | Implemented behavior |
|---|---|
| Layer 2 | Independent incidence analysis, recording-state comparisons, deterministic scene/SVG visualization, diagonal formal extension and metadata discovery; direct APIs and optional registered computations |
| Layer 3 | Registered structural types and representations; generic operators; shared expressions and Function composition; bounded validation/source reuse; batch host access; numerical/tabular, qualified history and CSR modules |
| Layer 4 | Sealed module/backend catalogs; prepared typed DAGs and provider selection; concurrent jobs, cancellation and admission; atomic record/artifact/history publication and recovery |
| Layer 5 | Immutable record values; extensible profiles; operand types and operation contracts; qualified relations and temporal projections; recording-history DAG, reconstruction, differences and explicit conflict resolution |
| Layer 6S | Memory/SQLite stores; immutable versions and objects; conditional atomic publication, retry and receipts; typed capabilities; verified bounded range readers |
| Layer 6E | Independent implementation registry; multiple providers per operation; explicit sequential/threaded CPU dispatch; contract checks, ordering, resource limits, cooperative cancellation and deadlines |
| SDK / CLI | Versioned C ABI, binary reading, Python plans/jobs/transactions, standalone JSON CLI, relocatable CMake package and separately packaged Python wheel |

These are trusted local libraries. Hashes establish content integrity within stated limits; attribution does not authenticate people or establish truth. Accelerator execution, distributed scheduling, access-control services and legacy migration remain unimplemented. Transactions cover one workspace on one backend; cross-store atomicity is not claimed.

## Build and check

On Windows, use Visual Studio 2022 with C++ tools/CMake and Python 3.10+. Native dependencies are pinned in-tree. Packaging tests use two pinned build tools in a local environment:

```powershell
python -m venv build/sdk-venv
build/sdk-venv/Scripts/python.exe -m pip install -r tools/python-build-requirements.txt
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
$env:GR_OS_LIBRARY = (Resolve-Path build/Release/gr_native.dll).Path
$env:PYTHONPATH = (Resolve-Path bindings/python/src).Path
python -B examples/python/layers_5_6.py
python -B examples/python/runtime.py
build/Release/gr.exe --help
```

The PowerShell flag is process-local. The helper discovers Visual Studio's CMake, builds and runs CTest. Packaging tests build/install offline using the prepared environment; no global Python package is changed. See [SDK instructions](docs/sdk.md) for offline setup and native-only options.

With CMake on PATH:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix build/install
```

Use separate build directories for different toolchains. Full validation requires Python discovery and the packaging environment. Linux/macOS interfaces are considered but not fully qualified by the Windows evidence.

## Source organization

```text
include/gr/              public support, kernel, storage, execution, runtime headers
src/data/                Layer 3 language, standard, numeric, sparse and relational modules
packages/               independent analysis, visualization and diagonal source packages
src/support/             canonical metadata, hashing and errors
src/kernel/              types, operations, records, projections and history
src/storage/             contracts, memory and SQLite adapters
src/execution/           implementation registry and CPU adapters
src/runtime/             catalog, planner, scheduler and transaction coordinator
experiments/data/        retained v0.4 compatibility consumer
src/cli/                 standalone native JSON command/session tool
bindings/c/              composition, JSON transport and binary C ABI
bindings/python/         installable standard-library Python facade
tests/                   unit, conformance and integration tests
examples/, benchmarks/   consumers and repeatable workloads
cmake/, tools/           packaging, build and installed-consumer checks
docs/, decisions/        contracts, review evidence and decisions
third_party/             pinned dependencies, manifests and notices
```

Layer 6S depends only on shared primitives. Layer 6E consumes L5 operand/operation contracts but does not depend on records or storage. Layer 4 composes native semantic admission, prepared execution and atomic physical publication. C/Python/CLI share that native path.

Layer 3 adds logical typing and composition above this foundation. It reuses the planner/scheduler and can operate without historical records or a database. Its mathematical extensions remain separate from kernel builtins and application methods.

Layer 2 packages build independently against the installed SDK. Their categories
are descriptive; ordinary CMake and Python packaging manage distribution.
[Performance evidence](docs/validation/benchmark-v0.7-analysis.md) and
[module contracts](docs/modules.md) distinguish metadata discovery from trusted
activation, native sharing from Python copies, and computational results from
historical or institutional admission. Portable snapshots validate structure and
digest consistency; complete source custody remains separate.
See [package build and composition instructions](docs/module-packaging.md) for
independent CMake installs, Python wheels and the cross-language consumer.

## Review entry points

- [v0.7 architectural review](docs/reviews/v0.7-review.md), [assessment and plan](docs/reviews/v0.7-assessment-and-plan.md), [decision](decisions/ADR-0007-independent-layer2-packages.md), [module contracts](docs/modules.md), [analysis API](packages/gr_relational_analysis/README.md), [visualization API](packages/gr_visualization/README.md), [installed C++ composition](tests/integration/modules/main.cpp)
- [v0.6 architectural review](docs/reviews/v0.6-review.md), [assessment and plan](docs/reviews/v0.6-assessment-and-plan.md), [decision](decisions/ADR-0006-layer3-maturation.md), [measured results and regressions](docs/validation/benchmark-v0.6-analysis.md), [numerical contracts](docs/numeric-data.md), [interoperability](docs/interoperability.md), [independent package](packages/gr_diagonal/README.md)
- [v0.5 architectural review](docs/reviews/v0.5-review.md), [assessment and plan](docs/reviews/v0.5-assessment-and-plan.md), [decision](decisions/ADR-0005-extensible-computational-language.md), [computational language](docs/data-language.md), [data values](docs/data-values.md), [qualified relational processing](docs/relational-data.md), [typed SDK](docs/data-sdk.md)
- [v0.4 architectural review](docs/reviews/v0.4-review.md), [initial assessment and plan](docs/reviews/v0.4-assessment-and-plan.md), [decision](decisions/ADR-0004-compact-history-admission-and-owned-buffers.md), [compact history](docs/history-storage.md), [buffers and numerics](docs/buffers.md)
- [Layer 4 architecture](docs/runtime.md), [assessment and plan](docs/reviews/layer-4-assessment-and-plan.md), [decision](decisions/ADR-0003-prepared-runtime-and-atomic-publication.md)
- [Planner and modules](docs/runtime-planner.md), [scheduler and resources](docs/runtime-scheduler.md), [transactions and ACID scope](docs/runtime-transactions.md), [runtime SDK/CLI](docs/runtime-sdk.md)
- [Layer 5–6 architecture](docs/layers-5-6.md), [v0.2 assessment](docs/reviews/architecture-v0.2-plan.md), [prior decision](decisions/ADR-0002-independent-contracts-and-foundation-reconstruction.md)
- [Records and history](docs/kernel-history.md), [execution contracts](docs/execution-contracts.md), [storage contracts](docs/storage-contracts.md)
- [SDK and installation](docs/sdk.md), [verification](docs/verification.md), [benchmarks](benchmarks/README.md)
- [Experimental historical-data consumer](experiments/data/README.md), [Python example](examples/python/historical_data.py)
- [Next decisions](docs/planning/open_questions.md), [contributor instructions](AGENTS.md), [change record](CHANGELOG.md)
- [Original assessment](ARCHITECTURE_REVIEW.md), [original evidence](REVIEW_EVIDENCE.md), [dependency provenance](third_party/README.md)

All reconstruction is under GR-OS. The [existing infrastructure](../gr_infrastructure/README.md), applications, handoff and programme publications remain reference material and have not been migrated.
