# Independent diagonal package

Experimental source package 0.2.0, requiring GR-OS 0.7.0. It is deliberately not
added as a subdirectory of the GR-OS build. Build it against an installed SDK:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/gr-sdk
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix /path/to/diagonal-sdk
```

The manifest declares exact dependency and exported logical/physical identities.
`find_package(GRDiagonal 0.2.0 EXACT REQUIRED)` imports `GRDiagonal::diagonal`. Call
`gr_diagonal::install(library)` after installing standard types and before sealing.
`gr_diagonal::make(coefficients)` constructs the mathematical value. The operator
`example.diagonal.apply@1` accepts it and a nonnullable int64 column. It composes
with ordinary `gr::data::Function`, standard operators and the existing runtime.
No central kernel switch or executor was added. Each coefficient multiplication
is checked for overflow; negative-stride views remain validated and supported.

Distribution is a **source API** boundary, requiring the exact installed SDK and
matching compiler, C++ standard library, runtime and configuration. The package's
CMake config rejects absent/wrong SDK versions. C linkage names in the narrow
facade do not establish cross-compiler C++ binary compatibility. Rebuild the
extension with the host's SDK/toolchain, including sanitizer configuration.

The Python wheel under `python/` has no third-party runtime dependency. It loads
the separately installed `gr_diagonal_native` through `GR_DIAGONAL_LIBRARY`:

```python
from gr_diagonal import Diagonal
result = Diagonal().apply([2, 3], [4, 5])
assert result.values == [8, 15]
print(result.report["provenance"])
```

This is a package-specific synchronous API. It runs the same installed native
language/compiler/runtime implementation, copies full-int64 input sequences,
returns ordinary caller-owned Python values and preserves native diagnostics.
It does **not** accept or produce `gr_os` opaque handles, install callbacks into
another engine, provide async job handles, or implement a universal plugin ABI.
C++ consumers have the full composition surface; Python consumers cross an
explicit copied-data boundary. The result report identifies package/dependency
metadata and the native plan/recipe. Metadata is not code authentication.

The canonical manifest is `python/src/gr_diagonal/manifest.json`, embedded by
CMake and included in the wheel. `gr_diagonal:manifest` is advertised through the
`gr_os.modules` entry-point group; metadata activation does not load the DLL.
Package 0.2.0 updates the SDK dependency and discovery metadata while retaining
the existing versioned mathematical type/operator/provider identities.

Inputs are bounded to 1,048,576 elements. `timeout_ms` bounds native job execution
only; it does not include Python conversion, native construction or planning.
Zero means no job deadline; values above 60,000 are rejected. Providers poll stop
requests during their loops. The synchronous call owns/drains its runtime before
returning. The facade has no long-lived native handle or unload API.

The code executes with the host process's privileges. Registration grants no GR
record access or publication authority, and this package is not sandboxed.
Conformance covers independent numerical expectations, malformed dimensions,
overflow, explicit composition and runtime deadline behavior. The installed
harness separately builds, installs, relocates, and consumes the native package
and offline-built wheel, including dependency failure checks.

`benchmark.py output.json` is a separate opt-in installed-Python measurement. It
checks an independent exact multiplication oracle, warms each case, and reports
five samples for 16, 4096 and 65536 elements. Its time includes Python and C copies,
native construction/planning, the one-worker runtime and result materialization.
It is a facade latency measurement, not isolated kernel throughput, and is excluded
from conformance timing gates.
