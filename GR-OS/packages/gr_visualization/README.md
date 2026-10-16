# GR Visualization

**EXPERIMENTAL independent Layer 2 package, version 0.1.0, for the exact GR-OS 0.7.0 C++ SDK.** This is an implementation pilot, not an adopted Standard or a claim about the geometry of GR. It consumes the qualified incidence projection exported by `gr_relational_analysis` 0.1.0 and produces deterministic structured scenes and SVG strings.

The package has no scheduler, storage writer, ontology registry, network client, dynamic plugin loader or browser application. Its native functions are ordinary C++20 functions. The separately packaged Python facade calls these functions through its package-owned ABI. Native C++ consumers can share a cheap immutable `NetworkSnapshot`; independent Python modules exchange an explicitly copied compact snapshot. They do not exchange GR-OS handles across DLLs.

## Native use

```cpp
#include <gr_visualization/visualization.hpp>

// network is an already validated gr_relational_analysis::NetworkView.
gr_visualization::ProjectionOptions options;
options.layout = gr_visualization::Layout::temporal_bands;
options.roles = std::vector<std::string>{"observer", "participant"};
auto scene = gr_visualization::project(network.snapshot(), options);
auto scene_json = scene.encode();
auto svg_text = gr_visualization::svg(scene);
// The caller chooses whether and where to publish these strings.
```

`ProjectionOptions::from_json` and `to_json` provide the same strictly checked configuration to native and Python callers. Unknown options, fractional or overflowing geometry, unknown layouts and unknown temporal classes fail. Role strings are not a universal vocabulary. Omitted/null filters mean all occurrences; empty filters mean no occurrences. Filter strings are sorted and deduplicated for a stable projection identity.

`Scene::document()` is a borrowed const reference valid while the scene lives; `to_json()` and `encode()` return owned copies. Copies of a `Scene` share immutable owned data. A scene can outlive the network and its source session. It does not retain the complete qualified source. The library exports scenes but does not import arbitrary scene geometry or raw SVG.

## Projection semantics

`incidence_columns` places participant-owner records on the left and referenced targets on the right. These are disjoint vertex roles even when both designate the same record/version. Shape and source mappings preserve this distinction. Each recorded participant occurrence has a separate edge; repeated roles and endpoints are never deduplicated or expanded into an endpoint clique. The full participant object, declared orientation, ordinal, role and owner version remain in the scene. Lines have no causal arrowheads.

`temporal_bands` groups owner records by the qualified categories `active`, `inactive`, `indeterminate`, `unscoped`, `unclassified`, `not_requested`. The class is taken from the source's temporal report and occurrence qualification. An isolated owner is classified using the same report. Category order and integer spacing are layout choices, not elapsed time, geometric distance or a new resolution of competing accounts. Target vertices do not acquire their owners' temporal classifications.

Role and temporal filters select **occurrences only**. All source vertices remain visible, including isolated owners and targets. Every excluded occurrence appears in `excluded_occurrences` with the applicable filter reasons. Source-level exclusions remain separately in `source_context`. Input limits cause an error; the package never silently samples or truncates the topology. Long display labels are shortened by Unicode code point and marked `label_truncated`; full identifiers and roles remain in source mappings. Labels are source identifiers, not invented descriptions of the objects.

Nodes and edges are ordered by stable source keys. Coordinates, control points, palette, font requests and generated SVG element IDs are deterministic. Small parallel groups get distinct curves. Occurrence labels are ordered by desired vertical position and occurrence key, then assigned lanes separated by at least 14 layout units when the canvas permits. The scene records desired/actual label coordinates and offsets. If there are more labels than spaced lanes, `label_placement.dense_fallback` explicitly reports compressed ordered lanes where overlap remains possible; no occurrence is dropped. Dense curves can also overlap. Lane ordinals and complete mappings remain available for another renderer. Font metrics and rasterization vary by viewer; this package promises deterministic scene/SVG bytes for the same input and options, not identical pixels across platforms. SVG's [text rendering model](https://www.w3.org/TR/SVG2/text.html) depends on fonts and rendering context.

## Identity, provenance and custody

The scene contains the snapshot ID, exact source spec/payload/provenance identity, physical/semantic revision pins, selection, temporal report, qualification and loss inventory. It carries its local `validation_basis`: `source_derived` for a native projection derived through the analysis session, `structural_only` for a portable imported snapshot. Import verifies schema and declared hashes, not the truth or omitted historical derivation of a source. A transmitted validation flag cannot promote local trust.

Native and imported projections have equal geometry and source mappings for equal snapshots, but their scene IDs deliberately differ with the validation basis. The scene hash covers its complete JSON document except `scene_id`. The format uses strict UTF-8 compact JSON with lexically ordered object keys and integer geometry, then SHA-256. This larger container is `gr.visualization.incidence-scene/1`; it does **not** redefine the size-limited `gr.canonical-json/1` contract. Hashes are content references, not signatures or authority.

Portable scenes/SVG omit full source wrappers, archive bodies, artifact bytes, full recording-history DAG and computational provenance detail. Source custody remains with the analysis owner or separately retained qualified value. Structured scene metadata records these losses. The same complete scene is embedded as escaped SVG metadata, making the graphic self-describing at this projection level without presenting it as an archival replacement.

## Bounds and safe export

Defaults admit 4,096 vertices, 16,384 occurrences, a 16 MiB serialized scene and a 32 MiB SVG. Caller limits can be smaller; hard ceilings are 110,000 vertices, 100,000 occurrences and 64 MiB per serialized output. Width is 640–32,768, row spacing 48–512, margins 16–128, and computed height is at most 1,000,000 layout units. Byte limits bound serialized outputs, not process RAM: JSON trees, stored encoded scene, caller copies, parsing and allocation overhead have additional costs. Scene creation copies compact context/mappings into a new document. SVG metadata escaping streams through the output bound.

Cancellation is cooperative: checked before work, through node/edge loops and metadata emission, and before return. Individual bounded JSON encoding, hashing, UTF-8 validation and sorting steps are not preemptible. An error returns no partial scene/SVG. Already returned values remain immutable.

SVG uses a fixed subset (`svg`, `g`, `defs`, `clipPath`, `rect`, `path`, `text`, `title`, `desc`, `metadata`) with no scripts, hyperlinks, external resources or user-supplied raw markup. The only URL references are generated internal clip paths. Node labels and titles are shortened to fit approximate available space and clipped defensively; font shaping can still affect readability. A secondary node label distinguishes record-version prefixes from subject designations. Generated element IDs do not use source identifiers. User/source text is escaped; malformed UTF-8 and characters disallowed by [XML 1.0](https://www.w3.org/TR/xml/#charsets) fail rather than silently changing identities. SVG's [document structure](https://www.w3.org/TR/SVG2/struct.html) permits descriptive metadata; that metadata is not treated as executable content.

## Verification and examples

`test_visualization` builds a synthetic qualified history with two versions of a target, repeated participation, a relation referenced by another relation, an isolated owner, an unknown temporal boundary and a reported order. It checks occurrence mappings, deterministic bytes, filters, temporal categories, native/imported validation distinctions, source lifetime, XML injection, malformed Unicode, strict configuration, resource failures and cancellation. This is focused conformance evidence for the implemented projection, not validation of an analytical interpretation.

`example_visualization OUTPUT_DIRECTORY` writes `incidence_columns.svg`, `temporal_bands.svg` and their `.scene.json` companions. Only this explicitly invoked example performs file I/O. All records are synthetic.

`benchmark_visualization` emits JSON to stdout. It measures native snapshot sharing, projection, copying scene bytes, SVG export, compact snapshot export/import, portable snapshot-to-SVG, and a synthetic publication-to-SVG pipeline at 32, 256 and 1,024 participating owner records. Fixture setup respects the store's declared records-per-commit limit, reserving one update for the tracked-history manifest: the 32/256 cases use two publications, while the 1,024 case uses three. Publication counts and the limit are reported per case; smaller cases retain their original single revised-publication history. Every sample checks its result. Ordinary phases use one warmup and five timed repetitions; the larger synthetic end-to-end phase uses one warmup and three repetitions. Samples, median and nearest-rank p95 are retained. At five samples p95 is the maximum, not a reliable tail-latency estimate. Byte counters describe serialized containers, not measured allocations or RSS. Run on an idle host separately from builds/tests. The analysis package's benchmark gives more detailed acquisition/incidence/index phases.

Install/build definitions and Python packaging are conventional package files beside this README. Consumers request exact compatible SDK/package versions. No claim of a stable C++ binary plugin ABI is made.
