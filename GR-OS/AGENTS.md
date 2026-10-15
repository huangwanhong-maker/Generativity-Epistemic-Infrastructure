# GR-OS contributor instructions

## Scope and current stage

**v0.6 is complete and stopped for architectural review.** Start with the
[v0.6 review](docs/reviews/v0.6-review.md), [verification](docs/verification.md)
and [performance evidence](docs/validation/benchmark-v0.6-analysis.md). All selected
ASan tests have passing evidence across the preserved initial run and corrected
installed-consumer rerun; do not erase the initial harness failure. Preserve
benchmark regressions and cache/transport/memory limits alongside improvements.
Do not start broad Layer 2 or migration without the next user instruction.

The current authorization is **v0.6 Layer 3 maturation**, superseding the v0.5 stop
point below. Read [the assessment and plan](docs/reviews/v0.6-assessment-and-plan.md)
and [ADR-0006](decisions/ADR-0006-layer3-maturation.md). Implement bounded immutable
validation reuse, measured batch/source-access improvements, numerical contracts,
and an independently buildable/installable extension with a small composed consumer.
Preserve the frozen v0.5 baseline and unchanged-workload comparisons. Do not begin
broad Layer 2 development or application migration. Stop for architectural review
after v0.6 verification and documentation. Cache identity is exact owner/spec/context,
not a caller-provided digest; native packages are trusted, not sandboxed.

This directory is the separate reconstruction workspace for GR infrastructure. The user explicitly authorized v0.5: a bounded Layer 3 computational language foundation after architectural assessment, validated with numerical/tabular, qualified relational-historical and extensible mathematical pilots. The [v0.5 assessment and plan](docs/reviews/v0.5-assessment-and-plan.md) records the selected direction. Implement, test and document it here, then stop for architectural review. Do not begin Layers 1–2 development, application migration, user-data migration, or changes to legacy infrastructure. This supersedes the v0.4 stop point; the [v0.4 assessment](docs/reviews/v0.4-assessment-and-plan.md) remains historical evidence.

Keep new implementation work in this workspace. Treat `../gr_infrastructure/`, its application repositories, existing runtime data, and the original handoff archive as reference material. Do not repair, move, rewrite, or delete them without an explicit instruction covering that change. Preserve unrelated changes in the parent repository.

## Orientation and authority

The parent [AGENTS.md](../AGENTS.md) applies here. Read [PROGRAMME_BRIEF.md](../PROGRAMME_BRIEF.md) before substantive programme work. Read the local [architectural assessment](ARCHITECTURE_REVIEW.md) and [open decisions](docs/planning/open_questions.md) for this workspace's discussion state.

The original handoff is `../gr_infrastructure/GR_Infrastructure_Architecture_R8_Codex_Handoff_2026-10-13.zip`. Its internal root is `GR_R8_Architecture_Codex_Handoff_2026-10-13/`. Start with its `README.md`, `AGENTS.md`, `architecture/00_R8_READ_FIRST.md`, architecture documents 25 through 32, and decision register; consult earlier proposals and source references for the relevant task. The adjacent `CONTRIBUTORS.md` records attribution and is not the architectural handoff.

The user selected R8's explicit layered organization and development layer by layer, with C++ for the foundation, Python access and scientific work, and TypeScript considered for higher application layers. [ADR-0001](decisions/ADR-0001-layered-development-and-layers-5-6.md) records this direction and the bounded first implementation. Other R8 details and the original assessment remain proposals. Do not interpret the layer diagram as requiring a compiler, VM, scheduler, backend, or domain ontology before a concrete contract needs one.

Preserve the programme's source hierarchy: governance procedure, adopted Standards for conformance, accepted ADRs for their stated architecture scope, foundation manuscripts for GR interpretation, then drafts/planning and examples. Software compatibility and institutional or standards authority are different claims. Consult actual foundation manuscripts when changing GR concepts or interpreting theoretical tensions.

## Semantic and historical discipline

- Distinguish represented subjects, record identities and versions, content digests, type identities, principals/signers, and physical locations.
- Keep represented occurrence, recording/admission, observation, and interpretation histories distinguishable. Do not infer occurrence order from commit order, or simultaneity from incomparability.
- Preserve qualified, potentially n-ary relations with participant roles and their own histories where needed. Do not equate a display edge with causation or endorsement.
- Keep assertion, evidence, interpretation, normative judgment, legal status, institutional recognition, and technical validation distinct. Cryptographic verification does not establish truth or authority.
- Treat the six operational record roles as an operational vocabulary, not ultimate reality or a mandatory programming-language class hierarchy.
- Preserve contradictory accounts, missingness, uncertainty, and declared projection losses. A small core still needs explicit, testable semantic contracts.

## Reconstruction and migration

Major replacement and complete reimplementation are legitimate future options. Existing source structure, frameworks, APIs, and backend choices do not receive automatic priority. Preserve useful algorithms and tests when they serve a clear contract.

Before migration, make an explicit GSP/GRRP compatibility matrix covering original bytes, canonicalization, identifiers, signatures and their coverage, temporal/parent relationships, attribution, disclosure, key custody, assets, unknown extensions, and partial mappings. Classify mappings as exact, qualified, lossy, unsupported, or unresolved. Never imply that a newly signed translation preserves the original signature's scope.

Use versioned legacy readers and retained originals when a faithful native mapping is unavailable. Keep account authentication, acting party, signer, registrar, and custody roles explicit. Do not copy private runtime data or keys into source or test fixtures. Rehearse any later data migration on authorized copies, with recovery and cutover evidence.

## Engineering practice

Prefer an independently usable foundation, public command/query contracts, clear owners for invariants, and complete, testable capabilities within the authorized layers. Applications and profiles can share history mechanics without sharing every domain rule. Keep scientific models and numerical arrays outside mandatory record semantics. Layer 5 owns semantic validation/projection; Layer 6S owns physical storage integrity/transactions; Layer 6E owns prepared physical execution. Layer 4 owns catalogs/plans, jobs/scheduling/resources, execution policy and semantic transaction coordination. Shared byte/JSON/error primitives do not depend on semantic code. C/Python/CLI reuse the native runtime instead of independently implementing admission.

Use C++20 for the current native libraries and a versioned C ABI with a standard-library Python facade. Current adapters are in-memory and SQLite storage, and sequential/threaded host CPU execution. Extend or replace adapters using concrete workloads and measured evidence. Do not build a custom compiler, VM, distributed scheduler, or universal plugin ABI merely to reproduce an R8 diagram. Separate data custody and transaction guarantees from analytical execution without assuming every storage format implements the same guarantees.

Use shared conformance fixtures where languages or backends claim the same behavior. Distinguish semantic preservation from byte equality and numerical equivalence. Version decoders, validators, profiles, projection recipes, and interfaces explicitly. Unknown content may be preserved safely without executing it or claiming its semantics have been validated.

Make authorization, concurrency, idempotency, recovery, resource limits, cancellation, and external effects explicit at their actual enforcement boundaries. A descriptor or an in-process plugin interface is not a sandbox. External actions need authorization covering that action; installation alone grants none.

## Traceability and reporting

For the reconstructed v0.2 layout, follow [ADR-0002](decisions/ADR-0002-independent-contracts-and-foundation-reconstruction.md) and the [architecture guide](docs/layers-5-6.md). Operand types, semantic operations and physical implementations have separate registries; operations pin type descriptors, and providers pin operation descriptors. Record profiles remain separate from computational operand types. Semantic checkpoint ancestry, physical commit history and represented event order are distinct. Do not claim semantic DAG support for a physical store that advertises linear history.

For v0.3, also read [ADR-0003](decisions/ADR-0003-prepared-runtime-and-atomic-publication.md) and [the runtime architecture](docs/runtime.md). Keep execution pure/exact until an explicit effect contract is implemented. Publication is a distinct barrier. Preserve exact frozen retries, unknown-outcome reporting and successful-commit knowledge; never silently rebase or rerun an operation. Synchronous convenience APIs must preserve a recovery handle when publication is uncertain.

Public headers belong under `include/gr`; sources under cohesive support/kernel/storage/execution/runtime directories; binding composition under `bindings`. Keep native tests grouped by unit/conformance/integration responsibilities. Full verification includes installed C/C++ consumers, CLI and the offline-built Python wheel, using packaging tools in the build-local environment. Benchmark when performance-relevant paths change; report measurement environment and transport costs. Timing regression gates are opt-in and do not replace behavioral conformance.

For v0.4, read [ADR-0004](decisions/ADR-0004-compact-history-admission-and-owned-buffers.md), [compact history](docs/history-storage.md), and [buffers](docs/buffers.md). Preserve v1 semantic node identities independently of physical encoding; existing v1 manifests retain append encoding unless explicitly upgraded. Distinguish selected reconstruction from full DAG audit. Keep writer admission optional and process-local, preserve physical CAS and frozen recovery, and never silently rebase an explicit base. Validation memo reuse requires exact wrapper equality and a pinned registry; candidate-wide dependencies remain checked.

Distinguish payload limits from process memory, logical view size from retained allocations, semantic-history DAGs from physical ledgers, cooperative cancellation from preemption, process termination tests from power-loss qualification, and policy checks from authentication/sandboxing. Compact history still has finite object/node limits, linear catalogue lookup and full physical snapshot membership. Plans/jobs/code objects and buffer views have explicit lifetime; unload with active workers is unsupported. Keep the experimental Layer 3 consumer separate from kernel builtins and domain ontologies.

Record substantive changes in local `CHANGELOG.md`. Record unresolved issues in `docs/planning/open_questions.md`. For an actual architecture decision, add an ADR under `decisions/` with issue, alternatives, decision/status, rationale, affected contracts, and migration/validation consequences. Do not label discussion recommendations accepted.

For v0.5, read [ADR-0005](decisions/ADR-0005-extensible-computational-language.md). Layer 3 owns logical types, representations, generic operator inference and lazy expressions; it lowers concrete specializations into existing Layer 4 plans. Keep logical identity separate from layout, and computational lineage separate from evidence or institutional acceptance. A sealed language library retains trusted extension callbacks; it is not a plugin sandbox. Exact family, representation and operation descriptions are pinned into bound contracts. Input templates bind immutable values explicitly. No implicit casts, floating-point reassociation, universal tensor ontology or graph flattening. Optional mathematical structures belong in extension modules, not kernel switches. Preserve the existing experimental v0.4 module as a compatibility consumer; do not retrofit it into fundamental semantics.

Preserve existing programme documents and publication identifiers. Propose standards changes separately from implementation changes, with their correct class and maturity. Retain imported notices, contributor attribution, and provenance when code is later incorporated.

Report files changed, relevant tests and exact outcomes, unresolved failures, migration limits, and measured performance only where measured. Existing test results establish their tested scope; they do not establish GRRP/GSP equivalence, standards adoption, factual truth, or production readiness.
