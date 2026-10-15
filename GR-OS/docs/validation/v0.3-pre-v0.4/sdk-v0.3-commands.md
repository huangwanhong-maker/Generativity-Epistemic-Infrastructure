# GR-OS 0.3 SDK validation commands

Executed on Windows with Visual Studio 2022 Community MSVC 19.44.35209.0,
bundled CMake 3.31.6, and Python 3.12.10. Paths below are relative to `GR-OS`.
The existing `build/sdk-venv` supplied the pinned offline wheel build tools; this
stage did not install packages globally or fetch new dependencies.

```powershell
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 `
  -DGR_BUILD_TESTS=ON -DGR_BUILD_BINDINGS=ON -DGR_REQUIRE_PYTHON_TESTS=ON `
  -DGR_BUILD_BENCHMARKS=ON
& $cmake --build build --config Release --parallel 4
& $ctest --test-dir build -C Release --output-on-failure
```

CTest includes native unit/conformance tests, real subprocess termination and
reopen tests, Python/C/CLI integration, executable examples, and a staged,
relocated installed package. The installation test builds an actual wheel offline,
installs it into a disposable local target, and runs C, C++, Python and CLI runtime
consumers against the relocated native SDK. See `ctest-v0.3-details.txt` for actual
consumer paths, commands, test output, and wheel SHA-256.

Native AddressSanitizer configuration:

```powershell
& $cmake -S . -B build-asan -G 'Visual Studio 17 2022' -A x64 `
  -DGR_BUILD_TESTS=ON -DGR_BUILD_BINDINGS=OFF -DGR_BUILD_CLI=OFF `
  -DGR_BUILD_EXAMPLES=OFF -DGR_REQUIRE_PYTHON_TESTS=OFF `
  '-DCMAKE_CXX_FLAGS=/fsanitize=address' '-DCMAKE_C_FLAGS=/fsanitize=address' `
  '-DCMAKE_EXE_LINKER_FLAGS=/INCREMENTAL:NO'
& $cmake --build build-asan --config Release --parallel 4
$env:PATH = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;' + $env:PATH
& $ctest --test-dir build-asan -C Release --output-on-failure
```

This instruments native support/kernel/storage/execution/runtime and native tests,
including the crash-test worker. It excludes the C binding, CLI and Python package.
MSVC emitted C5072/LNK4302 because this Release configuration omitted debug symbols;
address instrumentation remains enabled, but diagnostic symbol quality is limited.
This is not ThreadSanitizer, a leak audit, power-loss testing, or exhaustive proof
of memory/thread safety.

Idle-host benchmark commands (run sequentially, after builds/tests stop):

```powershell
$env:GR_OS_LIBRARY = (Resolve-Path build/Release/gr_native.dll).Path
$env:PYTHONPATH = (Resolve-Path bindings/python/src).Path
& build/Release/benchmark_runtime.exe docs/validation/benchmark-runtime-v0.3.json
python -B benchmarks/transactions.py --mode fixed --repeats 3 --output docs/validation/benchmark-v0.3-transactions.json
python -B benchmarks/run.py --output docs/validation/benchmark-v0.3.json --label v0.3 --repeats 5
python -B benchmarks/compare.py docs/validation/benchmark-v0.2-pre-runtime-preserved.json docs/validation/benchmark-v0.3.json --check
```

The original v0.2 benchmark was preserved byte-for-byte before code changes as
`benchmark-v0.2-pre-runtime-preserved.json` (SHA-256
`db30f688821bf728d2412d819b9d2fce7b678260b6c110e6ac29d497789241ae`).
The pre-edit complete 12-suite baseline is `ctest-v0.2-pre-runtime.txt`.
Early failed validation logs are retained separately: the first exposed a test
assuming immediate internal worker-lease cleanup; the next compiled a test against
a concurrently changing transaction implementation. Final builds used stable
sources and forced scheduler recompilation before acceptance.

The first transaction capture used eight commits per writer, which increased total
work, record count and history depth with writer count. It is retained byte-for-byte
as `benchmark-v0.3-transactions-growing.json`, SHA-256
`e5a53bb74f6bf88d25719fe9d09fdd39a9af0c2fd845adbb520e97d59ac96099`.
A subsequent isolated run used the explicit fixed-mode command above, from the
existing `build/sdk-venv/Scripts/python.exe`, to hold seeded state and total 32
accepted commits constant. Only the benchmark runner changed for that refinement;
the accepted native/SDK sources and binaries were unchanged.
