# Independent qualified relational analysis

Experimental source package **0.1.0**, requiring the exact **GR-OS 0.7.0** SDK.
This is a Layer 2 analytical method, not a semantic kernel, an adopted Standard,
or a claim that recorded relationships are true. It depends on the existing
Layer 3 qualified-incidence contract and reuses its historical validation.

Build against an installed SDK, independently of the GR source tree:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/gr-sdk
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix /path/to/analysis-sdk
```

Installed consumers use `find_package(GRRelationalAnalysis 0.1.0 EXACT REQUIRED)`
and `GRRelationalAnalysis::analysis`. The package manifest describes the pure
analytical exports. Matching C++ compiler, standard library, runtime and build
configuration are required; C linkage does not establish a cross-toolchain ABI.

## Native composition and ownership

Include `gr_relational_analysis/analysis.hpp`. Install the ordinary relational
module once and keep its returned Session. Historical acquisition remains an
explicit eager coordinator read; construct incidences through the existing
Layer 3 operations. The package accepts `gr.data.qualified-incidence@1` values,
not unqualified edge lists or records masquerading as incidences.

```cpp
namespace data = gr::data;
namespace rel = gr::data::relational;
namespace analysis = gr_relational_analysis;
auto library = std::make_shared<data::Library>();
auto session = rel::install(*library);
analysis::install(*library, session); // Optional scheduled summary capability.
library->seal();

auto records = rel::select_history(coordinator, {"workspace", semantic, physical});
auto expr = data::apply(library, rel::project_operation,
                       {data::literal(library, records)});
auto program = data::compile({expr});
auto incidence = program.materialize(runtime.run(program.plan())).at(0);
auto network = analysis::analyze(incidence, session);
auto report = analysis::summarize(network);
auto snapshot = network.snapshot();
```

`NetworkView` retains the exact input DataValue, copied computational provenance,
validated decoded source/derivation and indexed topology. It survives the input,
Session, Library and store being released. Analysis never consults a moving head.
`snapshot()` shares immutable topology/context without encoding or copying its
vectors. A snapshot can outlive the NetworkView; it does not retain the omitted
source payload. Its local `source_derived` basis records how that projection was
constructed, not continuing custody or external authentication.

`summarize(network)` and `compare(left, right)` are direct native calls. They do
not create a Runtime or plan on every request. The direct methods support a
`std::stop_token`; owned immutable inputs permit concurrent readers. Cancellation
is cooperative at scan points. Parsing, validation, hashing and serialization
remain bounded operations without preemption. Native code is trusted process
code, not sandboxed, and receives no publication or record-access authority from
registration. These methods perform no storage writes or external effects.

## The analyzed object

The topology is an undirected bipartite **recorded-incidence** multigraph:

- A participant-owner vertex identifies each current wrapper containing a
  `participants` array, including an empty array or a completely filtered owner.
  Unknown participant-bearing profiles remain owner records, not automatically
  ontology Relations.
- A target vertex identifies `(scope, ID, resolved record version or null)` for
  a selected occurrence. A subject designation is not resolved to a real-world
  person or entity. Only referenced targets enter this projection.
- An occurrence edge identifies `(owner record ID, owner version, ordinal)` and
  retains the complete participant object, exact role and owner temporal class.
  Repeated roles, repeated targets, parallel owner records, participant order
  and relations participating in other relations remain distinguishable.

Owner and target vertex kinds remain distinct even when their record/version
matches. No clique expansion or implied causal direction is introduced.
Opaque additional participant/reference fields remain retained and uninterpreted;
scope, ID and optional pinned version alone determine target resolution.
The source still retains qualifications, source payloads, archived references,
attribution and claims. Those distinctions are not converted to probabilities,
endorsement or a preferred account.

Role selection is already explicit in the input. The package retains excluded
occurrence keys and isolated owner vertices. Temporal classifications and the
complete interpretation report come from the Layer 3/5 query; recording ancestry
does not become occurrence order. Cycles or unknown required temporal semantics
can leave occurrences `unclassified` without discarding them. A class describes
the owner record, not the target participant. Absence in the projection does not
establish absence in the world.

## Statistics and comparisons

Summary reports contain exact integer owner/target/selected/excluded counts,
role and temporal-class distributions, each vertex's occurrence degree and
distinct-neighbor degree, and undirected connected-component inventories.
Repeated occurrences increase occurrence degree; they do not create new distinct
neighbors. Isolated owners form individual zero-edge components. These are
structural statistics of this declared graph, not counts of unique people, true
propositions or causal contributions. Density and centrality are not provided.

`compare` requires the same workspace and exactly the same role and temporal
query selections. Different workspaces need an identity mapping not implemented
here. It reports record IDs added, removed or changed in version; exact occurrence
keys added or removed; and changed resolution/participant/temporal data under an
unchanged occurrence key. For example, an unpinned target can acquire a new
record version while its owning participant occurrence remains unchanged.
An owner revision creates new occurrence keys; ordinal alignment is not silently
treated as persistent real-world occurrence identity. Summary deltas use signed
integers. The reports do not infer represented event order from recording changes.

## Snapshot interchange and validation

The versioned byte container `gr.analysis.incidence-snapshot/1` contains sorted
vertices and occurrences, source selection/pins, recording context, temporal
query/report, excluded keys and explicit qualifications/losses. Its identity binds
the full logical ValueSpec, exact source payload digest and computational
provenance digest. A separate snapshot ID covers the complete deterministic
portable container except its own ID field. Equal source bytes with different
computational provenance produce different source/snapshot identities.

The snapshot deliberately omits source wrappers, the direct archive and detailed
computational provenance. The selected source wrappers, direct archive, pins and
outer computational provenance remain available through `network.source().value()`
and its owning qualified view while the network lives. Neither that selected
source nor this snapshot contains artifact bodies or the full history DAG; those
require separately retained store/history custody. Pins and digests are custody
references; they cannot prove omitted derivation or authenticate an asserted
workspace.

`NetworkSnapshot::decode(bytes)` and `from_json(value)` check bounds, exact
container fields, identities, unique sorted vertices/occurrences, endpoint
consistency, selected/excluded disjointness, contiguous owner ordinal inventories,
role selection, temporal-class consistency and the container digest. Additional
reference annotations are opaque. Byte decoding rejects duplicate keys and
malformed UTF-8. Imported snapshots always have local `structural_only` status;
there is no wire field that upgrades this to source validation. A caller can
construct a structurally consistent fabricated snapshot; the parser makes no
historical authenticity claim. Visualization can therefore distinguish a direct
source-derived native snapshot from an imported projection.

## Layer 3 summary operator

`install(library, session, limits)` registers:

| Export | Identity |
| --- | --- |
| Logical summary family | `gr.analysis.incidence-summary@1` |
| Representation | `gr.analysis.summary-json-bytes@1` |
| Pure exact operator | `gr.analysis.incidence-summary@1` |
| Host CPU provider | `gr.analysis.cpu.incidence-summary@1` |

Use the same Session returned when installing the Library's relational profiles.
Install before sealing. The operator takes one qualified-incidence expression,
accepts no attributes, and composes through ordinary Expr/Function, prepared plans
and the sequential/threaded CPU providers. A threaded backend does not parallelize
one network scan internally. The package creates no second scheduler.

The provider receives payload carriers, not the outer DataValue provenance.
Its output `gr.analysis.prepared-summary/1` therefore identifies the source payload
digest and revision pins and explicitly declares carrier-scoped lineage.
`Program.materialize` attaches complete computational input/recipe/provider lineage.
It does not fabricate a full source-value identity from missing provenance.
Direct summary reports instead identify their actual NetworkSnapshot/source value.

`inspect_summary` validates the deterministic byte representation, field bounds,
degree/histogram conservation and per-component inventory/count consistency.
Without source edges it cannot independently prove component connectivity or
that statistics were correctly derived. Recompute from the retained qualified
input when that assurance is needed. Validation of a report is not acceptance
of its substantive interpretation. The native tests compare scheduled and direct
results against hand-computed expectations.

## Python boundary

The separately installed Python wheel loads the package DLL explicitly:

```python
from gr_relational_analysis import Analysis
analysis = Analysis(library="/path/to/gr_relational_analysis_native.dll")
with analysis.import_value(qualified_incidence) as network:
    report = network.summary()
    portable = network.snapshot()
    # Pass portable to the independently installed visualization package.
```

`import_value` reads authoritative native ValueSpec/provenance and copies the
qualified bytes once into a persistent module-owned native Network. The existing
main SDK byte export uses base64 JSON and has its own response limit; this initial
import is not zero-copy. Subsequent summary/compare calls reuse native source and
topology. Each result is a detached Python copy. Closing a Network releases its
handle; already acquired native calls hold their own shared owner.

`NetworkSnapshot.encode()` contains compact immutable bytes. Python package
composition copies that portable projection; it does not pass private gr_os
handles across DLLs. Snapshot import validates structure anew, independently of
Python object labels. The facade is blocking and has no Python cooperative
cancellation contract; native C++ does. Package loading does not provide a sandbox.

## Limits, evidence and measurements

Defaults and accepted maxima are 110,000 vertices, 100,000 total selected plus
excluded incidences, and 64 MiB per snapshot/report byte container. Lower positive
limits can be supplied natively. Existing 10,000-current/10,000-archive record and
per-record metadata bounds still apply. Byte containers permit results above
1 MiB; they are not passed through the bounded metadata canonicalizer. The strict
container permits at most 80 nesting levels and two million JSON values.
Excess inputs/results fail rather than being silently truncated. Temporary JSON
trees, indexes and serialization mean these limits are not process-RSS bounds.
Source-retaining views explicitly extend payload and decoded-tree lifetime outside
the Session cache quota. Snapshot exports and Python reports allocate copies.

The native test target covers structural oracles, repeated/parallel/higher-order
incidences, unknown profiles, selected exclusions, archived/current resolution,
recording comparisons, temporal cycles/unknown required extensions, provenance
identity, snapshot tampering, limits, lifetime, concurrent reads and native
cancellation requests. It also executes registered summaries through a reusable
Function and tests runtime output budgets. Packaging/relocation and Python evidence
are produced by the central installed-module harness; inspect its actual report
instead of treating this test inventory as a passing-result claim.

Enable `GR_MODULE_BUILD_BENCHMARKS` for
`benchmark_analysis [repeats] [fixture_directory]` (default 3,
allowed 1–7). It uses actual tracked 64/256/768-relation sources with 16 entity
records, three occurrences per relation and a second recording state. It reports
historical acquisition and initial Layer 3 projection separately from indexing,
shared snapshot access, repeated summaries/comparisons, portable encode/import and
prepared execution/materialization. Every sample checks pins/identities/counts
and repeated checksums. Byte counts and cumulative process peak RSS have different
scopes. No timing threshold is a conformance requirement; this is a bounded local
synthetic workload, not evidence for arbitrary graph scale.
The optional directory receives `incidence-N.bin`/`.json` and
`incidence-next-N.bin`/`.json` for each relation count N. These are exact payload
bytes and authoritative ValueSpec/provenance, written outside all timed phases
for independent Python and visualization measurements of the same inputs.

Conceptual sources are the same bounded sources documented in the GR-OS relational
pilot: FM-007 Part II pp.32–33 and 36–39 on contextual incidence, reidentification
and multiplicity; FM-005 Chapter 5 pp.14,29,168–169 on evidence and revisability;
and proposed M120-003–015/P140-002/005/014. Those manuscripts and Working Drafts
are interpretive/provisional sources, not adopted conformance authority. No
foundation definition, programme standard or legacy application is changed here.
