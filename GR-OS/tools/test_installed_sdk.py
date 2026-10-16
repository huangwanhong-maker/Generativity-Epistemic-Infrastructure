"""Install, relocate, and consume native/Python SDKs without source include paths.

Uses only local build tools and wheel dependencies. Never accesses the network or
installs globally. Artifacts remain under the selected build directory.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

parser = argparse.ArgumentParser()
parser.add_argument("--cmake", required=True)
parser.add_argument("--build", required=True, type=Path)
parser.add_argument("--source", required=True, type=Path)
parser.add_argument("--config", default="Release")
args = parser.parse_args()
build = args.build.resolve()
source = args.source.resolve()
run = build / "p" / uuid.uuid4().hex[:8]
run.mkdir(parents=True)


def execute(command, **kwargs):
    subprocess.run([str(part) for part in command], check=True, **kwargs)


stage = run / "stage"
execute([args.cmake, "--install", build, "--config", args.config, "--prefix", stage])
# Rename within the verified build directory: exported paths must be relocatable.
relocated = run / "relocated"
stage.rename(relocated)
consumer = run / "c"
cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")


def cached(name):
    return next(
        (
            line.split("=", 1)[1]
            for line in cache.splitlines()
            if line.startswith(name + ":")
        ),
        "",
    )


command = [
    args.cmake,
    "-S",
    source / "tests/integration/installed",
    "-B",
    consumer,
    "-DCMAKE_PREFIX_PATH=" + str(relocated),
    "-DCMAKE_BUILD_TYPE=" + args.config,
]
generator = cached("CMAKE_GENERATOR")
if generator:
    command.extend(["-G", generator])
if cached("CMAKE_GENERATOR_PLATFORM"):
    command.extend(["-A", cached("CMAKE_GENERATOR_PLATFORM")])
for name in ("CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS", "CMAKE_EXE_LINKER_FLAGS"):
    if "sanitize" in cached(name) or (
        name == "CMAKE_EXE_LINKER_FLAGS" and "sanitize" in cached("CMAKE_CXX_FLAGS")
    ):
        command.append("-D" + name + "=" + cached(name))
execute(command)
execute([args.cmake, "--build", consumer, "--config", args.config])
environment = os.environ.copy()
environment["PATH"] = str(relocated / "bin") + os.pathsep + environment.get("PATH", "")
sanitizer = None
if os.name == "nt" and "sanitize" in cached("CMAKE_CXX_FLAGS"):
    # Test-only deployment before any instrumented executable or DLL is loaded.
    # Global Python does not provide this toolchain dependency through its PATH.
    sanitizer = build / args.config / "clang_rt.asan_dynamic-x86_64.dll"
    if not sanitizer.is_file():
        raise RuntimeError(
            "Stage the installed MSVC ASan runtime beside the sanitizer build DLL first"
        )
    shutil.copy2(sanitizer, relocated / "bin" / sanitizer.name)
for name in ("consumer_c", "consumer_cpp"):
    candidates = [
        consumer / args.config / (name + ".exe"),
        consumer / (name + ".exe"),
        consumer / name,
    ]
    executable = next(path for path in candidates if path.is_file())
    if sanitizer is not None:
        shutil.copy2(sanitizer, executable.parent / sanitizer.name)
    execute([executable], env=environment, cwd=run)
venv = build / "sdk-venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
if not venv.is_file():
    # Share only the existing offline packaging tool environment across build
    # configurations; test imports still select the newly installed wheel.
    venv = (
        source
        / "build/sdk-venv"
        / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    )
python = venv if venv.is_file() else Path(sys.executable)
check = subprocess.run(
    [str(python), "-c", "import setuptools,wheel"], capture_output=True
)
if check.returncode:
    raise RuntimeError(
        "Packaging tests require pinned local build tools; see docs/sdk.md (create build/sdk-venv)."
    )
package_source = run / "python-source"
shutil.copytree(
    source / "bindings/python",
    package_source,
    ignore=shutil.ignore_patterns("__pycache__", "*.egg-info", "build", "dist"),
)
wheels = run / "wheels"
execute(
    [
        python,
        "-m",
        "pip",
        "wheel",
        "--disable-pip-version-check",
        "--no-cache-dir",
        "--no-index",
        "--no-build-isolation",
        "--no-deps",
        "--wheel-dir",
        wheels,
        package_source,
    ]
)
wheel = next(wheels.glob("gr_os-0.7.0-py3-none-any.whl"))
destination = run / "python-installed"
execute(
    [
        python,
        "-m",
        "pip",
        "install",
        "--disable-pip-version-check",
        "--no-cache-dir",
        "--no-index",
        "--no-deps",
        "--target",
        destination,
        wheel,
    ]
)
libraries = (
    list((relocated / "bin").glob("gr_native.dll"))
    + list((relocated / "lib").glob("libgr_native.so"))
    + list((relocated / "lib").glob("libgr_native.dylib"))
)
if not libraries:
    libraries = list(relocated.rglob("libgr_native.so")) + list(
        relocated.rglob("libgr_native.dylib")
    )
environment["GR_OS_LIBRARY"] = str(libraries[0])
if sanitizer is not None:
    # Test-only sanitizer deployment: ctypes' secure DLL search does not use
    # PATH. This installed toolchain dependency is not part of the release SDK.
    shutil.copy2(sanitizer, libraries[0].parent / sanitizer.name)
code = """import sys
sys.path.insert(0,sys.argv[1])
import gr_os as gr
assert gr.__file__.startswith(sys.argv[1])
assert gr.__version__ == '0.7.0'
assert gr.execute('identity',[b'installed']).outputs==[b'installed']
with gr.Runtime() as runtime:
    graph=gr.Graph([gr.Node('installed','gr.bytes.identity@1',[gr.Value.bytes(b'wheel')])],[gr.OutputRef('installed')])
    result=runtime.run(graph).raise_for_status()
    assert result.outputs[0].data==b'wheel'
with gr.Buffer(b'installed-readonly') as buffer:
    assert buffer.to_bytes() == b'installed-readonly'
    from pathlib import Path
    import os
    siblings = Path(os.environ['GR_OS_LIBRARY']).parent
    if list(siblings.glob('_gr_buffer*.pyd')) + list(siblings.glob('_gr_buffer*.so')):
        exported = buffer.memoryview()
    else:
        exported = memoryview(buffer.to_bytes())
assert exported.readonly and exported.tobytes() == b'installed-readonly'
exported.release()
if 'data_language' in gr.Native().capabilities.get('features', []):
    from gr_os import data
    from contextlib import ExitStack
    with ExitStack() as stack:
        context=stack.enter_context(data.Context(modules=('gr.data.standard@1','gr.data.numeric@1')))
        runtime=stack.enter_context(gr.Runtime())
        value=stack.enter_context(context.column([1,None,3]))
        literal=stack.enter_context(context.literal(value))
        total=stack.enter_context(context.apply('gr.data.sum@1',[literal],attributes={'nulls':'skip','min_count':1}))
        function=stack.enter_context(total.function())
        program=stack.enter_context(function.bind())
        result=stack.enter_context(program.run(runtime)).raise_for_status()
        unsigned=stack.enter_context(context.scalar(2**64-1,dtype='uint64'))
        assert unsigned.tolist()==2**64-1
        assert result.outputs[0].tolist()==4
print('Relocated C/C++ and installed Python wheel runtime/buffer consumers passed')
"""
execute([python, "-I", "-B", "-c", code, destination], env=environment, cwd=run)
cli = relocated / "bin" / ("gr.exe" if os.name == "nt" else "gr")
if cli.is_file():
    import json

    response = subprocess.run(
        [str(cli), "run"],
        input=json.dumps(
            {
                "graph": {
                    "nodes": [
                        {
                            "id": "installed",
                            "operation": "gr.bytes.identity@1",
                            "inputs": [
                                {
                                    "literal": {
                                        "type": "gr.bytes@1",
                                        "representation": "bytes",
                                        "data_base64": "Y2xp",
                                    }
                                }
                            ],
                        }
                    ],
                    "outputs": [{"node": "installed", "output": 0}],
                }
            }
        ),
        text=True,
        capture_output=True,
        env=environment,
        cwd=run,
        check=True,
    )
    assert json.loads(response.stdout)["result"]["outputs"][0]["data_base64"] == "Y2xp"
    print("Relocated native CLI runtime consumer passed")
