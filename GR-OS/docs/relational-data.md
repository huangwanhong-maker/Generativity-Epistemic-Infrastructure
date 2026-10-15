# Qualified relational data pilot

Publication class: experimental implementation specification. Maturity: **EXPERIMENTAL**, GR-OS v0.6. This module is not an adopted GR Standard or a claim of conformance to the provisional GR-SPEC-121 temporal model.

The Layer 3 relational module provides one bounded, reusable historical-data workflow: select a recording revision, project its participant incidences, select explicit roles, and count the selected occurrences. It uses the common logical-type/representation/operator library and the existing Layer 4 planner and runtime. It introduces no second scheduler, storage engine, query language parser, or domain-specific research method.

## Native interface

Include `gr/data/relational.hpp` and link `GR::data_relational`. Install `gr::data::relational::install(library)` before sealing the common library. An optional copied `core::TypeRegistry` installs the record-profile context used for validation and temporal interpretation. Its callbacks are trusted native code; descriptor fingerprints do not prove callback correctness.

`select_history(coordinator, HistoricalSelection{workspace, semantic_revision, physical_revision})` returns a `DataValue`. This is an eager storage read, performed outside the expression graph. An empty physical revision pins the current physical head during that call. An empty semantic revision selects that pinned manifest's semantic head. Both actual identities and the manifest version are returned in provenance. Physical revision identifiers are opaque, workspace-scoped storage identities; semantic revisions and manifest wrapper versions have their separate content-digest contracts. Later expression execution never consults the coordinator or a moving head.

```cpp
namespace data = gr::data;
namespace rel = gr::data::relational;
auto library = std::make_shared<data::Library>();
auto session = rel::install(*library);
library->seal();
auto pinned = rel::select_history(coordinator, {"workspace", semantic_revision, ""});
auto records = data::literal(library, pinned);
auto incidences = data::apply(library, rel::project_operation, {records});
auto actors = data::apply(library, rel::roles_operation, {incidences},
                         {{"roles", {"actor"}}});
auto counts = data::apply(library, rel::count_operation, {actors});
auto program = data::compile({counts});
auto result = runtime.run(program.plan());
auto values = program.materialize(result);
auto qualified = session->inspect(values.at(0));
```

All three operators are pure and exactly deterministic for the installed profile context. The providers support the existing sequential and threaded CPU backends. Threaded selection does not parallelize one incidence projection internally; the runtime can schedule independent expression nodes concurrently. No implicit casts or factual, causal, legal, or epistemic adjudication occur.

| Logical family | Meaning |
| --- | --- |
| `gr.data.qualified-records@1` | A selected recording snapshot, direct archive, recording-history context and revision pins. |
| `gr.data.qualified-incidence@1` | Exact participant occurrences plus explicit role selection and optional represented-time interpretation. |
| `gr.data.incidence-counts@1` | Structural occurrence counts retaining the complete incidence view and contributor keys. |

These families have no type arguments or parameters in this version. Their installed representation is `gr.data.qualified-json-bytes@1`: immutable deterministic UTF-8 JSON **bytes**, rather than a metadata JSON operand. Complete sources can therefore exceed the common per-metadata-value 1 MiB limit.

| Operator | Input → output | Attributes |
| --- | --- | --- |
| `gr.data.incidence@1` | records → incidence | `{}` or `{"temporal": {"order_ids": [...], "after": [...]}}`; either inner field is optional. |
| `gr.data.select-incidence-roles@1` | incidence → incidence | `{"roles": [...]}` with unique exact, nonempty role strings. |
| `gr.data.count-incidences@1` | incidence → counts | `{}`. |

Selection applies to participant roles, not to a record's separate envelope `roles` field. Repeated selections intersect the previous selection. An empty roles array selects nothing; it does not mean unrestricted. Unknown attributes fail. A role absent from the source yields no selected occurrences and leaves the omitted occurrence inventory visible.

## Preserved semantics and identity

Each value retains the complete selected current wrappers, direct pinned-reference archive, history parents and attribution, and physical/semantic/manifest provenance. Artifact descriptors, qualifications, observers/methods, opaque extensions, neutral references, payloads and represented temporal bounds remain in those source wrappers. The value does not contain artifact bytes or the full recording-history DAG.

An occurrence key is `(relation record ID, relation record version, participant ordinal)`. N-ary roles, participant order, repeated participation, parallel relation records and relations on relations remain distinct. The word “relation” in an occurrence key denotes the owner of the envelope's participant array; an unknown record profile with participants remains structurally inspectable without claiming that its payload meaning has been understood.

Every selected occurrence contains the full original participant object, a separate resolution descriptor, and the owner record's temporal classification. A pinned record reference resolves its exact version, including the supplied archive. An unpinned record reference resolves the version current in the selected **semantic** snapshot. A subject reference remains a subject designation with a null record version. Historical wrappers' own dependencies are not recursively rebound or expanded; this preserves the existing Layer 5 direct-archive contract.

The count grouping key is `(reference scope, ID, resolved record version or null, participant role, temporal class)`. It deliberately aggregates incidence occurrences with that same key. Each group retains the exact contributing occurrence keys, and the full incidence and source inventories remain accessible. Distinct orientations or claim qualifications can occur in the same count group; inspect the contributor incidences and source records before making an interpretation that depends on them. A structural count is not a count of true propositions, real-world relationships, unique people, or causal contributions. Subject-designation grouping does not establish shared real-world identity.

Each projected value explicitly lists selected and excluded incidence keys. The absence of selected occurrences is not evidence that no corresponding event, relation, or participant exists. Neutral envelope references are retained in source wrappers and are not counted as participant incidences.

## Two temporal contexts

Recording-history parents and physical storage revision are provenance, never represented event order. The optional `temporal` attribute delegates to `core::temporal` with the installed record profiles and preserves the entire query and report, including included/excluded orders, diagnostics, unsupported constraints, consistency and interpretation completeness.

An explicit `after` set must already be downward closed under the selected supported precedence claims. This module does not add predecessors automatically. That is the current Layer 5 operational contract; it differs from closure construction described in earlier provisional standards material. No conformance equivalence is asserted.

When a cut is established, an incidence receives its **owner record's** `active`, `inactive`, `indeterminate`, or `unscoped` class. These classes do not classify the participant target. Unknown endpoints, unbounded endpoints, absent temporal scopes and incomparable event bounds remain different in the retained source and report. No temporal request produces `not_requested`; a request without an established cut produces `unclassified`.

Cyclic order claims or selected unknown required order semantics do not manufacture a cut. All participant occurrences and source accounts remain available. Selecting a supported subset of orders is explicit and records exclusions. Neither contested qualifiers nor confidence text are silently converted into acceptance, probability, or a “best” chronology. No wall-clock chronology, timezone conversion, event-time database index, or interval optimization is introduced.

## Validation, lineage and limits

Source validation verifies complete record wrappers and references, then recomputes the selected semantic history descriptor from its normalized record/archive inventory, parents and attribution. This detects altered wrappers or an inconsistent selected semantic identity. It does not authenticate an externally supplied workspace name, physical revision claim, recorder, or source truth; the eager coordinator read supplies those pins through the existing trusted storage boundary.

Registered representation validation recomputes the derived incidence selection and counts from the retained source and compares deterministic bytes. It rejects forged counts, exclusions, resolution versions, classifications and container fields. The shared bounded strict JSON container parser rejects duplicate keys. Per-record Layer 5 canonical metadata constraints still apply. The common `Program.materialize` path adds expression/provider/plan lineage; the relational payload keeps source and interpretation context independently available. Displaying or exporting only the `counts` array discards that context and is a further, explicitly lossy consumer transformation.

The module caps a payload at 64 MiB, participant incidences at 100,000, selected roles at 1,024, role strings at 512 bytes, and the decoded container at 80 nesting levels and two million JSON values. Layer 5's current record/archive and metadata limits apply as well. Planner, invocation and job payload budgets may impose lower bounds. Input and output are owned immutable byte carriers; there is no zero-copy claim.

The v0.6 implementation retains a bounded parsed source and derived view for successful immutable payload owners. New owners are still decoded and checked; equivalent sources can reuse validation only after exact deterministic source-byte comparison inside the same pinned profile context. Role selection and counting reuse validated incidence inventories. The portable output still contains the complete source and requires serialization and an owned byte allocation. Derivation is linear in records and incidences apart from ordered lookups/grouping and temporal analysis. Temporal queries additionally retain the existing Layer 5 graph traversal costs. Declared payload limits do not bound transient JSON allocator overhead or total process RSS. Cancellation is cooperative at operation boundaries and record/participant loops; parsing, hashing and Layer 5 validation are bounded but are not preemptible. Large graph analytics, query indexes, sparse-matrix conversion, streaming relational operators and separately referenced portable source objects are deferred until measured workloads justify their contracts.

## Scoped validation reuse and profiling

`install(library, registry, CacheOptions)` returns a shared `Session`; existing callers can ignore it. The default parsed-state memo permits 64 entries and a 16 MiB conservative serialized-state charge. Either zero limit disables retention; accepted maxima are 1,024 entries and 256 MiB. `profile_timing` defaults to false. A session pins a copied record-profile registry and supports `inspect(value)`, `statistics()` and `clear_cache()`. Installed callbacks retain that context even if the public session handle is released. The free `relational::inspect(value, registry)` remains independent, uncached full validation.

Owner hits require the same live immutable carrier owner and exact logical family; these families admit no arguments or parameters. Weak owner tokens distinguish reused addresses without extending the raw payload lifetime. Decoded source/view state remains retained until pruning, eviction or explicit clear; pruning occurs during inspection, callback validation and statistics reads. A new owner is parsed strictly, its declared derivation is recomputed, and its complete deterministic bytes are compared. Source reuse uses the provenance only as a lookup hint and requires exact source-wire equality with a previously successful source in this session. A claimed revision or caller-provided digest is never a certificate. Failed checks are not retained. Separate installations cannot inherit certificates from different profile registries. Generic `Library` validation reuse is a separate, also bounded mechanism.

The source document and record/version indexes are immutable and shared among retained views. Serialization inserts the validated source's existing deterministic bytes rather than deep-copying and serializing that JSON tree again. This is an internal optimization: format, operator/type IDs, historical identity, qualification lists and exact result bytes remain v1-compatible. New operator outputs undergo independent representation validation before reuse. Source acquisition still uses the coordinator and storage integrity checks; no memo hides later storage corruption or substitutes a moving head for an eager pin.

The charge is raw qualified wire size plus detached-view serialized size plus source-wire size for each entry; shared sources are conservatively charged per entry. This bounds serialized-state retention, not decoded JSON allocation overhead, active operations, allocator caches or process RSS. Weak payload lifetime does not make retained parsed trees free. `clear_cache()` releases entries and advances a generation so an already running validation cannot repopulate the cleared generation. Concurrent first readers can perform duplicate validation; results remain immutable and publication is synchronized. Counters remain cumulative after clear, and a statistics snapshot taken during concurrent execution is observational rather than a transactional event log.

Statistics report hits, misses/full checks, rejected checks, source reuse/checks, decoded/serialized bytes, derivations, operator calls, evictions, expired owners and current/peak charges. Optional nanosecond totals cover decode, source validation/index construction, derivation and encode. They do not account for every allocation, comparison, lookup, materialization or JSON inspection copy. `Session::inspect` returns an independent JSON copy even on a memo hit. Timing collection adds overhead; there is no timing-based correctness gate.

## Conformance scope

`tests/conformance/data_relational.cpp` exercises pinned old/new recording revisions and distinct physical pins, archived participant versions, n-ary/repeated/parallel/higher-order occurrences, subject designations, qualifiers, role omissions, contributor-count conservation, serial/threaded equality, source-store immutability, retrospective order, cycles, unknown required extensions, absent/unknown/unbounded temporal scopes, forged data rejection, logical-family/attribute rejection, a source above 1 MiB, and an invocation input cap. Additional v0.6 cases exercise same-owner reuse, new equal-byte owners, transport retagging, wrong logical family, forged data after warm-up, duplicate JSON keys, independent registries, cache-off byte equality, repeated role intersections, byte/entry bounds, eviction, weak lifetime, explicit clear and concurrent reads/clear. These tests establish the bounded implementation contract; they do not establish factual accuracy of source records or standards adoption.

Conceptual alignment is deliberately narrow. Sources actually consulted are identified below; PDF page numbers are one-based file pages. These are interpretive sources and provisional requirements, not a claim that the pilot implements a foundation theory or establishes standards conformance.

- [FM-007 in the foundation manifest](../../foundation_manuscripts/MANIFEST.md#fm-007), [*A Metamodel for Ontologically Heterogeneous Social Dynamics, Part II*](<../../foundation_manuscripts/A Metamodel for Ontologically Heterogeneous Social Dynamics-II.pdf>): PDF pp. 32–33, including §8.2 on contextual incidence and n-ary roles; pp. 36–37 on relational persistence, explanation and scope; pp. 38–39, §§9.1–9.3, distinguishing candidate transitions, occurrence tokens and losses from retaining only transition types. The manuscript is marked **DRAFT**. These passages support preserving distinct occurrence keys and avoiding endpoint-only or causal interpretations of a projected incidence.
- [FM-005 in the foundation manifest](../../foundation_manuscripts/MANIFEST.md#fm-005), [*Epistemology of Generative Relational Economics*, Chapter 5 first draft](../../foundation_manuscripts/GRE_Chapter_05_Complete_First_Draft.pdf): PDF p. 14 (§5.2.1) distinguishes observed manifestation, conditions, evidence and auxiliary assumptions; p. 29 (§5.4.1) distinguishes relations, traces and their evidential roles; pp. 168–169 (§5.20 and Definition 5.159) connect contestability and revisability to retained provenance. This pilot preserves relevant descriptions; it does not evaluate their warrant.
- [GR-120 Working Draft](../../standards/GR-120-information-model/GR-120.md), proposed M120-003–009 and M120-010–015, informs identifiable qualified relations, repeated incidences, opaque modules and temporal distinctions. [GR-140 Working Draft](../../standards/GR-140-presentation/GR-140.md), proposed P140-002, P140-005 and P140-014, informs historical content resolution, visible filtering and indeterminate temporal views. Both documents explicitly state that they are standards-track Working Drafts, **not adopted Standards**.
- [GR-200 Working Draft 0.3 source](../../standards/GR-200-epistemic-recording/GR-200.tex), proposed E200-008, E200-013–015 and E200-033, informs multiple temporal meanings, missingness, uncertainty, competing accounts and versioned transformation provenance. Its maturity is **PREPARATORY; PROVISIONAL**. [GR-SPEC-121](../../specifications/GR-SPEC-121-event-time-projection/GR-SPEC-121.md), T121-013, describes predecessor-closure construction; the explicit-cut difference from this implementation is recorded above rather than silently equated.

No foundation definitions, programme drafts or source documents are changed by this pilot.

## Measurement protocol

`benchmark_data_relational [repeats]` builds actual tracked in-memory workspaces with 64 or 256 relation records, 16 entity records and two recording revisions. Each relation has three explicit occurrences, including a repeated actor. The measured source is the first semantic revision selected through a fixed newer physical manifest; setup is outside the phase timings. The default is three repeats, including the first repeat.

The JSON report separates eager historical selection, literal validation, expression construction/compilation, one prepared runtime execution, and result materialization plus full qualified inspection. It checks selected/excluded occurrence counts, unchanged storage head, revision pins and a deterministic counts checksum. Raw samples, phase medians, source/result payload sizes and cumulative process peak RSS are retained. Peak RSS includes setup and earlier cases; it is neither per-phase memory nor a fresh-process peak for each case. The benchmark includes real validation/copying costs and performs no temporal query. It does not isolate a raw graph kernel, measure SDK transport or SQLite I/O, or isolate arithmetic from validation. The benchmark source remains unchanged from v0.5 so preserved v0.5 and current v0.6 runs can be compared on the same workload.


`benchmark_data_relational_profile [repeats]` is a separate instrumented v0.6 diagnostic with 64, 256 and 768 relation records. Each size selects one actual tracked source and runs the same incidence/role/count pipeline with both caches disabled, both cleared before each pipeline, or one untimed warm-up pipeline. New output owners still receive complete validation in warm mode. Literal, expression/compile, prepared execution, materialization, scoped inspection and independent uncached inspection have separate wall times. Per-phase relational counter/timing deltas and generic-cache snapshots expose where reuse occurs. The independent inspection uses a separate uninstrumented context; its wall time is measured but is absent from the installed session counters. Counts checksums, source pins and independent decoded output equality are checked in every mode. Setup and eager selection are reported outside pipeline timings. This diagnostic includes instrumentation overhead and is distinct from the unchanged v0.5 workload comparison.
