# Building and composing independent packages

Experimental implementation guidance, v0.7. These are trusted source packages,
not adopted standards or a universal binary plugin ABI. The package manifests
describe capabilities and exact dependencies; they do not authenticate code or
grant access to records. See [discovery and activation](modules.md).

The three packages build independently from an installed GR-OS 0.7.0 SDK:

| Source package | CMake package / target | Python distribution | Version |
|---|---|---|---|
| `gr_relational_analysis` | `GRRelationalAnalysis` / `GRRelationalAnalysis::analysis` | `gr-relational-analysis` | 0.1.0 |
| `gr_visualization` | `GRVisualization` / `GRVisualization::visualization` | `gr-visualization` | 0.1.0 |
| `gr_diagonal` | `GRDiagonal` / `GRDiagonal::diagonal` | `gr-diagonal` | 0.2.0 |

Visualization additionally requires GRRelationalAnalysis 0.1.0. The main build
does not add these packages as subdirectories or enumerate their operators in
the core C facade. C++ consumers explicitly install relational and analysis
operators, standard operators and diagonal operators into the same Library,
then seal it. Direct analysis/visualization calls use their ordinary native APIs.

## Source and installation boundary

Use the same compiler, C++ runtime, configuration and sanitizer settings as the
installed SDK. Exact CMake dependency checks reject declared version mismatches;
they are not proof of cross-compiler binary compatibility. Public C++ layouts may
change across SDK increments. The package-specific C interfaces have ABI version
1 and own their allocations, but still reuse the linked native SDK implementation.

For example, after building and installing GR-OS to `sdk`:

```sh
cmake -S packages/gr_relational_analysis -B build-analysis -DCMAKE_PREFIX_PATH=/absolute/sdk
cmake --build build-analysis --config Release
ctest --test-dir build-analysis -C Release --output-on-failure
cmake --install build-analysis --config Release --prefix /absolute/analysis-sdk

cmake -S packages/gr_visualization -B build-visualization "-DCMAKE_PREFIX_PATH=/absolute/sdk;/absolute/analysis-sdk"
cmake --build build-visualization --config Release
cmake --install build-visualization --config Release --prefix /absolute/visualization-sdk

cmake -S packages/gr_diagonal -B build-diagonal -DCMAKE_PREFIX_PATH=/absolute/sdk
cmake --build build-diagonal --config Release
cmake --install build-diagonal --config Release --prefix /absolute/diagonal-sdk
```

The manifests are canonical package data at
`python/src/<package>/manifest.json`. Each CMake build embeds and installs that
same file; each wheel includes it. `manifest()` reads metadata without loading a
native DLL. Python entry points in `gr_os.modules` advertise these factories.
Native code also exposes package-local `manifest()` functions; this introduces
no central header registry or callback loader.

Wheels contain standard-library Python facades, not native DLLs. Build them with
the pinned local setuptools/wheel tools described in [the SDK guide](sdk.md),
using `pip wheel --no-index --no-deps --no-build-isolation`. Install them into a
chosen environment or build-local target. Select the separately installed native
libraries explicitly through constructor arguments or `GR_ANALYSIS_LIBRARY`,
`GR_VISUALIZATION_LIBRARY`, and `GR_DIAGONAL_LIBRARY`. Installing a wheel or
enumerating its metadata does not load those libraries. Do not unload a library
while its objects or calls remain active.

## C++ and Python composition

The [installed C++ consumer](../tests/integration/modules/main.cpp) constructs
qualified historical incidence data, runs the registered analysis summary through
a native Function/Runtime, retains a NetworkView, renders a Scene/SVG and composes
an explicitly declared degree-weighting model with the diagonal Function. A
NetworkView retains its validated qualified source. Its NetworkSnapshot shares
immutable topology with native visualization without JSON serialization.

The Python boundary is deliberately different:

```python
from gr_relational_analysis import Analysis
from gr_visualization import Visualization

analysis = Analysis()  # explicit native library selection, possibly through env
visualization = Visualization()
with analysis.import_value(qualified_incidence_value) as network:
    summary = network.summary()
    snapshot = network.snapshot()
scene = visualization.render(snapshot, title="Recorded participation")
svg_text = scene.svg
```

`qualified_incidence_value` is an existing `gr_os.data.DataValue`. `describe()`
returns authoritative native specification/provenance, then `payload()` exports
immutable bytes. That existing main-SDK byte export currently uses base64 JSON
internally, with its response bound; importing near the 64 MiB source limit may
therefore fail earlier at that boundary. `Analysis.import_parts` accepts explicit
metadata and raw bytes directly. Both routes validate actual source semantics in
the analysis package. Import copies once and creates a retained native Network;
subsequent summary/comparison calls do not create a Runtime or re-import source.
The Python Network retains that source internally but does not expose the native
`NetworkView::source()` inspection/export surface. Retain the original gr_os
DataValue or `import_parts` bytes when later source-record/archive inspection or
export is needed; the compact snapshot is not a replacement for them.

`network.snapshot()` copies a compact portable projection, omitting source
wrappers, archive, artifact bytes and complete historical derivation. Its public
type is immutable bytes plus descriptive identity. `Analysis.decode_snapshot`
validates external bytes. Visualization always validates the actual portable
bytes again, including their digest and structure, and reports `structural_only`.
A digest does not prove the omitted derivation. It never trusts Python labels or
casts another DLL's handle. Consequently, a native source-derived scene and a
Python portable scene have different validation-basis provenance, even when their
geometry matches. Scene descriptions are detached copies; SVG and encoded scene
bytes remain valid after the originating Network is closed.

The facades use package-specific synchronous C calls. Native C++ operations
accept stop tokens, but these Python calls do not promise live cancellation or
preemption. Calls release the GIL. A close racing with a call may reject a handle
not yet acquired; a call that acquired its native shared owner remains valid.
Network context managers/finalizers release handles. Each DLL frees its own
responses; the Python wrapper retains the loaded library for live owners.

Analysis C metadata is limited to 1 MiB and raw source/snapshot input to 64 MiB.
Each loaded analysis DLL shares one sealed relational Library/Session and handle
registry across its Python Analysis clients. The facade permits 64 outstanding
or in-flight Network reservations and 256 MiB
of reserved source/metadata bytes, releasing that charge when the last native
call owner disappears. This is not an RSS or topology-allocation bound. Native
analysis separately limits vertices, occurrences and serialized snapshots.
Visualization has its own vertex, occurrence, scene and SVG limits. Transport
responses are bounded as well; a response failure rolls back a newly created
analysis handle. No raw pointer registration is exposed.

## Reproducible validation

`module_packages` invokes [the standalone harness](../tools/test_module_packages.py).
It copies source packages out of the main checkout, installs and relocates all
dependencies, builds native suites/examples, builds installed-only C++ consumers,
and installs offline wheels before running Python composition and lifetime tests.
All products stay under a unique `build/m/<id>` directory, including commands,
stdout/stderr and artifact hashes. Sanitizer runs propagate compiler/linker flags
and stage the existing toolchain runtime only inside that test directory.

No baseline SDK from an ignored directory is a prerequisite. Missing SDK probes
restrict CMake search to an empty root; incompatible version declarations are
checked against actual current installed configs. These negative declarations do
not constitute a historical SDK rebuild. The optional `--older-sdk` argument
performs a separately identified rejection probe against an explicitly supplied
historical installation. The diagonal harness likewise no longer requires the
old v0.5 installation. Local compiler/CMake and offline wheel tools remain real
prerequisites; this is not a dependency downloader.

The Python installed consumer checks native/Python source identity, summary and
portable scene parity, unchanged source custody, manifest parity, malformed
snapshots, explicit provenance changes, close/race behavior, handle exhaustion,
and actual package composition. Benchmarks are separate opt-in executables and
scripts; test timing is not used as a performance claim.
