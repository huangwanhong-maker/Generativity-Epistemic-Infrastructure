# Public SDK, ownership and local installation

**Class:** Experimental implementation specification  
**Version:** GR-OS 0.6; C ABI 1 with additive runtime, buffer and data operations

Runtime/plan/job/transaction ownership, Python asynchronous access, and the native
CLI are documented in [runtime-sdk.md](runtime-sdk.md).
The native typed data language and its Python/JSON facade are documented in
[data-sdk.md](data-sdk.md).

## Native libraries

Public C++20 headers mirror responsibilities:

| Headers | CMake target | Responsibility |
|---|---|---|
| `gr/support/common.hpp` | `GR::common` | JSON values, strict metadata parsing, canonical bytes, SHA-256, structured errors |
| `gr/memory/buffer.hpp` | `GR::buffer` | Immutable host owners, checked views, bounded allocation and buffer sets |
| `gr/kernel/types.hpp`, `operations.hpp` | `GR::semantics` | Versioned operand and operation contracts |
| `gr/kernel/numerical.hpp` | `GR::semantics` | Explicit numerical policies and checked integer primitives |
| `gr/kernel/records.hpp`, `projections.hpp`, `history.hpp` | `GR::core` | Record admission, projections, recording-history DAG |
| `gr/kernel/compact_history.hpp` | `GR::core` | Bounded immutable checkpoint/delta history reconstruction |
| `gr/storage/contracts.hpp`, `blob_reader.hpp` | `GR::storage` | Transactional history and independent binary-reader contracts |
| `gr/storage/memory_store.hpp`, `sqlite_store.hpp` | `GR::storage` | Selected concrete storage adapters |
| `gr/execution/implementation.hpp`, `backend.hpp`, `cpu.hpp` | `GR::execution` | Physical providers and explicitly selected CPU execution |
| `gr/runtime.hpp`, `gr/runtime/*.hpp` | `GR::runtime` | Sealed catalog, graph planning, bounded scheduling, transaction coordination |
| `gr/data/language.hpp` | `GR::data` | Immutable typed expressions, representation binding and lowering to runtime plans |
| `gr/data/standard.hpp`, `sparse.hpp`, `relational.hpp` | `GR::data_standard`, `GR::data_sparse`, `GR::data_relational` | Independently installed concrete type/representation/operator modules |
| `gr/c_api.h` | `GR::native` | Exception-safe C transport, binary reading and owned buffer leases |
| `gr/experimental/data.hpp` | `GR::experimental_data` (optional) | Explicitly installed historical integer-column experiment |

Compatibility umbrellas `gr/common.hpp`, `core.hpp`, `storage.hpp` and
`execution.hpp` remain. C++ APIs intentionally evolved at 0.2, 0.3 and 0.4; compiled C++ ABI
compatibility is not promised. Build a consumer with the installed matching
compiler/runtime and package. The C ABI isolates Python from C++ object layouts.

```cmake
find_package(GR 0.6 REQUIRED COMPONENTS core storage execution)
target_link_libraries(my_program PRIVATE GR::core GR::storage GR::execution)
```

The package exports C++20 requirements and transitive dependencies. It includes
the pinned public JSON header, native archives/shared library and third-party
notices. It performs no dependency download during CMake configuration/build.
`GR::semantics` depends on buffers and support; storage has no semantic dependency; execution
depends on semantic descriptions but not record projections or storage.

## Build, package and test

Windows requires an installed Visual Studio 2022 C++ toolchain and Python 3.10+.
The helper discovers bundled CMake when it is absent from PATH. Prepare isolated
Python packaging tools once, entirely inside `GR-OS/build`:

```powershell
python -m venv build/sdk-venv
build/sdk-venv/Scripts/python.exe -m pip install -r tools/python-build-requirements.txt
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

The pinned packaging dependencies are setuptools 80.9.0 and wheel 0.45.1. They
are build tools, not Python SDK runtime dependencies. For an offline machine,
prepopulate the venv from an authorized local wheel directory using `--no-index
--find-links <directory>`; neither CTest nor the installed-consumer test downloads
packages. The helper does not alter execution policy or global Python packages.

Other hosts can use CMake directly, or the `unix-release` preset. Create the same
`build-directory/sdk-venv` using that host's Python and install the pinned build
tools there. Native-only builds may explicitly disable bindings; Python tests
otherwise require interpreter discovery by default. `GR_REQUIRE_PYTHON_TESTS=OFF`
allows native verification when Python is unavailable; report that reduced scope.

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix build/install
```

CTest includes vendor hash verification and an installed SDK test. That test
installs under its build directory, relocates the installation, builds independent
C and C++ consumers, builds the Python wheel offline from a disposable source
copy, installs it to an isolated directory, and imports it with Python's isolated
mode. Test artifacts remain under the short `build/p` path for inspection (avoiding
Windows MSBuild path limits in deeply nested workspaces). Source
headers and `PYTHONPATH` are not used by those consumers.

For a user-selected virtual environment, build/install the Python facade:

```text
build/sdk-venv/Scripts/python.exe -m pip wheel --no-build-isolation --no-deps --no-index --wheel-dir build/wheels bindings/python
<your-venv-python> -m pip install --no-deps --no-index build/wheels/gr_os-0.6.0-py3-none-any.whl
```

On Unix substitute `build/sdk-venv/bin/python`. The `py3-none-any` wheel contains
only Python; the native SDK is installed separately. It does not claim bundled
Windows/Linux/macOS support. Pass `Native(path)` or set `GR_OS_LIBRARY` to the
installed `gr_native.dll`, `libgr_native.so` or `libgr_native.dylib`. No implicit
current-directory or network library discovery occurs.

## Python API

`construct`, `verify`, `validate`, `apply`, `graph`, `project` and `match` delegate
to native record semantics. `parse_metadata(text)` uses the strict metadata
parser; duplicate decoded keys, invalid UTF-8, unsafe numbers and structural
limits are rejected. Metadata parsing is distinct from permissive Python
`json.loads`; existing `gr.canonical-json/1` canonical bytes remain unchanged.

`history_validate`, `history_append`, `history_reconstruct`, `history_diff` and
`history_merge` operate on explicit `gr.history/1` bundles. They do not create a
hidden repository, infer represented occurrence order, or publish a transaction.
An unresolved merge returns conflicts; a resolution is an explicit record choice.

```python
from gr_os import Native, Storage, Value, execute_typed

native = Native("/absolute/path/to/libgr_native.so")
result = execute_typed(
    "gr.bytes.concat@1", "gr.cpu.concat@1",
    [[Value.bytes(b"left"), Value.bytes(b"right")]],
    backend="cpu.threaded", max_threads=2, native=native,
)
assert result.outputs[0][0].data == b"leftright"
```

`Value.json(type_id, data)` and `Value.bytes(data, type_id="gr.bytes@1")` encode
typed arguments. Each invocation supplies an ordered argument list, and the
batch returns ordered result lists. `contracts()` exposes built-in type,
operation and implementation descriptors. `execution_capabilities(backend)` and
`devices()` expose the selected physical interface. Custom validator/provider
registration is currently a trusted C++ facility; JSON descriptors do not install
code and the Python facade does not accept Python callbacks.

`execute("identity", buffers)` pins `gr.bytes.identity@1` to
`gr.cpu.identity.copy@1`. `execute("sha256", buffers)` pins
`gr.bytes.sha256@1` to `gr.cpu.sha256.oneshot@1`. The version argument remains
`"1"`. These convenience defaults do not select a faster provider dynamically.
`execute_typed` can explicitly select the incremental SHA-256 implementation;
metadata reports the operation contract and actual provider.

## Storage and binary ownership

`Storage(path, backend="sqlite")` and `Storage(backend="memory")` own process-local
native handles. Methods include `create`, `head`, `snapshot`, `record`, `commit`,
`history`, `receipt`, `blob`, `blob_info` and `open_blob`. Commits require explicit
expected revision, transaction ID, actor attribution and reason. Attribution does
not authenticate a principal. The Layer 4 transaction coordinator performs semantic admission;
raw C++ storage can intentionally retain opaque records.

Use context managers or explicit `close()`. A finalizer provides best-effort
cleanup for forgotten handles; it cannot report cleanup errors reliably.
Concurrent explicit closes are idempotent. Already acquired native operations
retain their resources; an operation racing with closure before acquisition can
fail with `invalid_handle`. A closed resource cannot be reopened.

```python
with Storage("example.sqlite", native=native) as store:
    reader = store.open_blob("workspace", "sha256:<digest>", chunk_bytes=65536)
# Reader owns a pinned verified object independently of the originating store.
with reader:
    buffer = bytearray(65536)
    size = reader.readinto(buffer, offset=0)
    for chunk in reader.chunks():
        consume(chunk)  # Each yielded chunk owns its Python bytes.
```

`blob_info` returns unverified length metadata; `open_blob` verifies the complete
object before exposing pinned ranges. A range is trusted through that retained
verified object, not independently authenticated by its global SHA-256 digest.
`readinto` borrows a contiguous writable Python buffer for the synchronous call;
do not access or mutate it concurrently. Reads return at most one configured
chunk and return zero at EOF. Beyond-EOF offsets fail. Reader cancellation is
permanent and cooperative; subsequent reads fail. C calls serialize reads per
reader. Reader/store limits are explicit; neither handles nor digests confer
authorization.

Uploads and byte-operation batches still use bounded JSON/base64 transport.
The new binary reader removes base64 from repeated artifact reads. Multipart
uploads, device arrays and zero-copy scientific interchange
remain unimplemented.

## C transport, errors and limits

`gr_call_json(request, size, response, response_size)` accepts UTF-8 JSON with
integer `abi: 1` and operation `op`. It returns an allocated envelope with `abi`,
`ok` and `result` or `error {code,message,details}`. Return codes are 0 success,
1 reported failure and 2 invalid response pointers/allocation failure. Always
release a non-null response with `gr_free`, including error responses. Input
buffers remain caller-owned; no C++ exception crosses the ABI.

JSON operation families are `core.*`, `history.*`, `store.*`, `blob.*`,
`execution.*`, `runtime.*`, `plan.*`, `job.*`, `transaction.*` and `metadata.parse`. Store/reader handles are process-local opaque
strings. Obtain a reader through `store.blob.open`, then call
`gr_blob_read(handle, handle_size, offset, destination, capacity, response,
response_size)`. Its JSON result reports `bytes_read`; bytes go directly into
caller-owned destination storage. `blob.cancel` and `blob.close` operate on that
reader handle. `store.receipt` exposes accepted transaction metadata without
rerunning current semantic validation.

Requests are bounded to 16 MiB/depth 64, and responses to 64 MiB. Metadata values
also obey their separate canonical limits. Responses are materialized JSON;
transport limits are not a process-wide memory quota. Python checks batch/payload
budgets before base64 allocation, and native decoding checks configured input
bytes before allocation. Existing whole-blob convenience reads have tighter
effective JSON limits than native storage.

A response allocation failure can follow an already accepted mutation. It is an
unknown outcome at the transport boundary, not proof of rollback. Recover using
the original transaction identity and receipt/retry contract. `NativeError.code`
and `.details` expose reported native failures; Python type/transport errors are
ordinary Python exceptions.

CPU cancellation/deadlines remain cooperative. Output limits apply to returned
payloads, not arbitrary allocations inside trusted callbacks. Native extensions
must honor their lifetime/effect contracts; no sandbox, authorization service or
network service is introduced. Layer 4 scheduling is described in [runtime-sdk.md](runtime-sdk.md).

Live execution cancellation through a stop token is currently a native C++ facility.
The C/Python execution adapter supports an initial cancelled flag and a relative
deadline only; the separate runtime job API exposes live asynchronous cancellation. Python blob readers
do support live cancellation through their reader handle.


## Owned buffers (v0.4)

`GR::buffer` exposes `gr/memory/buffer.hpp` without a Python dependency. Physical
layout and mathematical type remain distinct. C++ immutable Buffer/View aliases
retain shared allocations; mutable builders refuse freeze while a borrow is live.
See [buffer contracts](buffers.md) for layout, slicing, allocation and numerical
contract limits.

The C ABI adds `gr_buffer_import`, `gr_buffer_read`, `gr_buffer_pin` and
`gr_buffer_unpin`. Import copies caller bytes (64 MiB maximum); its bounded layout
contains `element`, `shape`, optional byte `strides`, and `byte_offset`. Readinto
copies contiguous logical bytes to caller storage. Pin returns a separate opaque
lease with a const pointer and shape/strides. All pointers/metadata remain valid
until unpin, even after closing the source handle. The native library must stay
loaded. C callers must respect constness and synchronize lease destruction.

The bridge permits 4096 exported view handles. A shared 256 MiB allocation budget
charges imported/copied owners until all views, plans and export leases release
those owners. This is not an RSS limit or a global quota for arbitrary native
providers. Native scheduler budgets additionally account for retained buffer owner
sizes; descriptor/transport/container/caller costs remain separately scoped.
Failed JSON response construction rolls back newly exported buffer handles.

```python
import struct
from gr_os import Buffer

layout = {"element": {"format_id": "gr.int64@1", "item_size": 8,
                      "alignment": 8, "byte_order": "little"}, "shape": [3]}
with Buffer(struct.pack("<3q", 10, 20, 30), layout=layout) as values:
    result = values.memoryview()
assert result.readonly
assert result.tolist() == [10, 20, 30]  # Native little-endian host example.
result.release()  # The view retained its owner after Buffer.close().
```

Python imports copy arbitrary exporters before asynchronous use. A readonly
memoryview does not prove its backing owner is immutable. The optional native
CPython exporter implements a genuinely readonly buffer protocol object; its
`.obj` exposes no writable alias. It pins the actual native lease until the last
view releases, including strided/negative-stride child views. Exact known int64
encodings map to PEP 3118 integer formats; unknown or parameterized formats remain
opaque items. Non-native-endian views may require copying/conversion in consumers.
No NumPy dependency or NumPy/Arrow/DLPack conformance claim is introduced.

`GR_BUILD_PYTHON_BUFFER=ON` opportunistically builds this small stable-ABI 3.11+
module when local development headers are present. The native install places it
beside the shared GR library. The pure Python wheel remains portable and contains
no compiled extension; it finds the optional exporter using the explicitly selected
native library directory and verifies exact linked-library identity. Python 3.10
and installs without the exporter use `Buffer.to_bytes()`/`readinto()` instead.
The buffer protocol owner lifetime follows [CPython's buffer contract](https://docs.python.org/3/c-api/buffer.html).

`BufferSet` holds named views and bounded metadata; `Value.buffers(type, carrier)`
passes that carrier into the same typed native execution/runtime APIs. Inputs must
use the same Native instance as their runtime. Plans retain native owners after
Python buffer handles close. Decoded output Value objects own handles: call
`Value.close()`, `BufferSet.close()` or the owning `JobResult.close()`/context manager.
Closing a BufferSet explicitly closes its contained Python handles, including any
that the caller supplied to its constructor.

For CLI sessions, `buffer.import_base64` and `buffer.export_base64` provide a bounded
copying fallback (12 MiB decoded export maximum). Handles are process-local and
must be consumed inside that session; raw C/Python binary transfer avoids base64.
`buffer.describe` returns diagnostic layout fields (`offset`, owner bytes, etc.);
construct import layouts explicitly with `byte_offset` rather than treating this
report as a portable serialized allocation.

