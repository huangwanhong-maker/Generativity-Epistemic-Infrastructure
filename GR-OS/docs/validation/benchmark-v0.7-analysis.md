# v0.7 performance and transport assessment

Class: local engineering measurements. Maturity: experimental. Date: 2026-10-17 JST.
These captures establish bounded behavior on this workstation, not production
throughput, statistical significance or a general speedup/nonregression guarantee.

## Method and preserved comparisons

Windows x64 / MSVC 19.44 Release, Intel Core i7-11370H, eight reported logical
processors, Python 3.12.10 and actual NumPy 2.5.0. Builds, tests and other agent
benchmarks had stopped. No affinity, power-state, temperature or background-service
control was imposed. Substantial run-to-run variation remains in the evidence.
The four shared benchmark source files are byte-identical to the preserved v0.6
archive. The runners' old hardcoded version labels (including Python's 0.5.0)
identify the unchanged harness; command/native hashes identify the actual v0.6
and v0.7 artifacts. They are not current library-version reports.

The [common driver](../../tools/measure_v07.py) runs data in old/new/new/old order,
then historical, runtime and Python cases serially. See
[commands](benchmark-v0.7-commands-final.json),
[all 80 derived comparisons](benchmark-v0.7-comparison.json), and
[70 matching output/identity checks](benchmark-v0.7-output-consistency.json).
Native data uses one warmup/five samples; historical processing uses three samples
including the first; runtime uses seven samples; Python uses one warmup/five.
Raw captures retain every sample and their original workload definitions.

## Existing workloads: retained gains and observed regressions

All times below are median milliseconds. Separate paired values are deliberately
retained rather than hiding the slower or warmer run in an aggregate.

| Workload | v0.6 | v0.7 | Observation |
|---|---:|---:|---|
|262,144-row nullable table, prepared serial, pair1|39.978|20.221|Initial old capture was substantially slower|
|Same, reverse-order pair2|20.673|20.778|1.005x; prior large serial-workload improvement is retained here|
|Same table, four workers, pair1|20.847|25.199|1.209x slower|
|Same, four workers, pair2|18.581|21.914|1.179x slower|
|16,384-row CSR direct prepared, pair2|0.880|0.920|1.045x slower|
|64-relation expression/compile|4.560|6.562|1.439x in this three-sample capture|
|256-relation prepared transformation|100.403|98.499|0.981x|
|256-relation materialize/inspect|51.056|56.371|1.104x slower|
|256 one-node runtime jobs|12.443|13.512|1.086x slower|
|Python262,144 nullable import|99.377|107.536|1.082x slower|
|Python262,144 result-to-list|56.159|62.305|1.109x slower|
|NumPy262,144 copy import|5.777|5.889|1.019x|
|NumPy262,144 read-only export|0.169|0.165|0.980x|

The native table comparison and large historical transform do not return to
pre-v0.6 costs. That does not establish a blanket performance improvement.
Four-worker table execution, several small setup paths and Python transport show
regressions in these captures. The existing runtime/provider implementations and
these runner sources are unchanged; no causal attribution to the new view API is
established. Fixed workload output digests and historical pins remain equal.
Cumulative historical-process peak working set at 256 relations was 30.51 MiB old
and 29.49 MiB new; this includes setup and earlier cases, not per-phase memory.

Runtime timing was particularly variable. The initial two-worker hash case changed
13.283 to 20.543 ms, but a separate new/old reverse-order diagnostic changed 17.412
to 14.699 ms. The serial hash case changed 23.322 to 25.115 ms initially and 25.034
to 24.641 ms on reversal. The one-node job case remained about 8% slower in both
captures. Preserve these observations rather than interpreting a single ordering
as an architectural gain or a proven source-level regression. See
[reverse commands](benchmark-v0.7-runtime-reverse-commands.json),
[new capture](benchmark-v0.7-runtime-reverse-for-v0.7.json), and
[old capture](benchmark-v0.6-runtime-reverse-for-v0.7.json).

## New native analysis

The [accepted capture](benchmark-v0.7-analysis-native-corrected.json) uses actual
tracked MemoryStore histories, 16 entity targets and three participant occurrences
per relation. Setup includes source construction, both recording states and prepared
computation. Acquisition and initial Layer 3 projection are reported separately.
Index timings construct a fresh NetworkView over an already validated owner/session;
they are not cold byte-import timings. Three samples are retained without a separate
phase warmup. Summaries/comparisons use that indexed owner. Snapshot sharing copies
an immutable handle; portable export/import performs real serialization/validation.

|Owners|Index|Summary|Compare|1,000 snapshot shares|Export|Import|Prepared summary/materialize|
|---:|---:|---:|---:|---:|---:|---:|---:|
|64|21.185|0.683|4.513|0.1696|2.603|14.889|27.391|
|256|99.661|2.127|18.306|0.1026|15.728|61.575|122.694|
|768|431.470|7.808|100.796|0.2121|52.686|454.051|523.633|

At 768 owners the qualified incidence is 1,290,888 bytes and the snapshot 1,024,267
bytes; the snapshot omits source wrappers/direct archive and full derivation.
It still contains qualified context and can exceed the underlying selected-source
JSON size. Cumulative peak working set reached 73.27 MiB, including setup and earlier
cases. This is not a per-Network allocation measurement or an RSS quota.
The earlier [analysis capture](benchmark-v0.7-analysis-native-final.json) is also
retained: the 64-owner index was 16.922 ms there versus 21.185 ms in the accepted full
module capture. Small microsecond-scale handle timings are local microbenchmarks,
not portable latency guarantees.

## New native visualization

The [visualization capture](benchmark-v0.7-visualization-native-corrected.json)
uses a different topology from analysis: distinct subject targets, repeated
observers, a historical target version, higher-order participation, temporal
classes and an isolated owner. Times therefore cannot be compared directly to the
Python analysis-fixture rows below as language overhead. Ordinary phases use one
warmup/five samples; publication-to-SVG uses one warmup/three. At five samples,
nearest-rank p95 is simply the maximum, not a reliable tail estimate.

|Participating owners|Occurrences|Scene projection|SVG export|Portable snapshot to SVG|Synthetic publication to SVG|Scene MiB|SVG MiB|
|---:|---:|---:|---:|---:|---:|---:|---:|
|32|97|9.369|7.122|33.300|90.558|0.107|0.222|
|256|769|63.886|61.385|486.972|1509.181|0.815|1.697|
|1024|3073|366.830|295.330|1276.452|3453.062|3.258|6.788|

End-to-end includes synthetic record construction, tracked MemoryStore publications,
qualified acquisition, Layer 3 incidence computation/materialization, analysis,
projection and SVG. It uses two publications at 32/256 owners and three at 1,024.
The backend permits 1,000 updates per commit; the fixture reserves one for the
history manifest and batches at 999 user records. These are real transaction costs.
The initial 1,024 fixture exceeded that limit and failed before any accepted visual
measurement. Its [failure log](benchmark-v0.7-visualization-native-final.txt) and
[original commands](benchmark-v0.7-modules-commands-final.json) remain preserved.
The corrected fixture passed a new 1,024-owner conformance case in both Release
and ASan. No production limit or algorithm was changed.

A 1,024-owner portable render is about 1.28 s in this capture and produces 6.79 MiB of
SVG, including source-mapping metadata. This is useful bounded export, not an
interactive large-graph latency claim. Native sharing avoids full topology copies;
scene construction, text serialization and portable input validation still allocate
and scale with output. Dense layouts can remain visually crowded.

## Installed Python composition and transport investigation

[Installed Python measurements](benchmark-v0.7-modules-python-corrected.json)
consume the exact 64/256/768 qualified source fixtures emitted by native analysis.
Every row includes result/identity checks outside timing, one warmup and five
samples. Calls include copying/response decoding and result destruction. Persistent
summary/compare reuses the native Network; import creates and closes a new owner.

|Owners|Raw import/close|Main SDK bridge/close|Retained summary|Retained compare|Snapshot export|Portable render|Raw import + summary + render|
|---:|---:|---:|---:|---:|---:|---:|---:|
|64|22.398|27.231|0.639|4.032|3.323|37.444|70.471|
|256|100.841|109.682|2.339|15.713|16.522|164.214|276.213|
|768|298.451|786.530|22.243|85.599|69.492|1072.088|1805.060|

The large sequential capture initially suggests a 488 ms bridge premium, but a
[focused alternating-order profile](benchmark-v0.7-python-bridge-profile.json)
does not support attributing that difference to base64 export. It measures native
metadata description at 0.193 ms and payload export/copy/close at 26.317 ms; raw import
median 764.425 ms and bridge import 668.505 ms have overlapping 405-808 / 421-822 ms ranges.
The ordinary raw-import run was only 289-377 ms. These later variations are preserved,
not replaced with the favorable sample. The profile isolates wall-clock phases,
not CPU samples, allocation counts or the cause of host/session variability.

Code inspection confirms that the main SDK exports the byte carrier via its existing
base64 JSON path, while the package accepts a raw byte pointer plus bounded metadata.
That extra export is a real copied boundary. Its 26 ms diagnostic is materially smaller
than full import/validation/indexing, and it does not explain the entire earlier gap.
The new module payload is qualified JSON bytes, not a NumPy tensor; changing NumPy
promotion or numeric kernels would not address this path. Persistent owners avoid
repeat source import, but portable visualization still parses and copies its input.
No zero-copy Python or cross-DLL handle sharing is claimed.

## Consequences and reproduction

Reuse retained native views for repeated methods; retain the original source when
custody is needed. Export only when crossing a real boundary. Choose an actual
workflow and scale target before shared Python provider registration, streaming
sources, incremental topology, binary scene encoding or GPU kernels. The measured
serialization/validation/output costs and variable runtime scheduling should guide
that decision. No unrelated optimization was introduced to explain noisy captures.

Run [the module driver](../../tools/measure_v07_modules.py) against an accepted
Release package run, after builds/tests stop; use a fresh label to preserve evidence.
It verifies native/wheel hashes and records fixture hashes and commands:
[accepted module commands](benchmark-v0.7-modules-commands-corrected.json).
[The bridge profile](../../tools/profile_v07_bridge.py) reproduces the targeted
alternating-order diagnostic. Source fixtures and all binaries are identified in
those records. The separate [verification report](../verification.md) records
behavioral acceptance, source/artifact hashes and platform limitations.
