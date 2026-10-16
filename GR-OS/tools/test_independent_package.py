"""Build/install a source package against only a relocated installed GR SDK.

All configuration, negative dependency checks, wheel creation and consumption stay
inside this build tree. No network, global install, or GR source include paths.
"""

import argparse
import hashlib
import json
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
build, source = args.build.resolve(), args.source.resolve()
run = build / "x" / uuid.uuid4().hex[:8]
run.mkdir(parents=True)
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


events = []


def execute(command, *, expected_failure=False, **kwargs):
    command = [str(part) for part in command]
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        **kwargs,
    )
    events.append(
        {
            "command": command,
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
    )
    if expected_failure:
        if not result.returncode:
            raise AssertionError(
                "Dependency rejection unexpectedly configured successfully"
            )
    elif result.returncode:
        print(result.stdout)
        print(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, command)
    return result


generator = ["-G", cached("CMAKE_GENERATOR")] if cached("CMAKE_GENERATOR") else []
if cached("CMAKE_GENERATOR_PLATFORM"):
    generator += ["-A", cached("CMAKE_GENERATOR_PLATFORM")]
flags = ["-DCMAKE_BUILD_TYPE=" + args.config]
for name in (
    "CMAKE_C_FLAGS",
    "CMAKE_CXX_FLAGS",
    "CMAKE_EXE_LINKER_FLAGS",
    "CMAKE_SHARED_LINKER_FLAGS",
):
    if "sanitize" in cached(name) or (
        "LINKER" in name and "sanitize" in cached("CMAKE_CXX_FLAGS")
    ):
        flags.append("-D" + name + "=" + cached(name))
environment = os.environ.copy()
environment.pop("CMAKE_PREFIX_PATH", None)
environment.pop("GR_DIR", None)

try:
    stage = run / "stage"
    execute(
        [args.cmake, "--install", build, "--config", args.config, "--prefix", stage]
    )
    sdk = run / "sdk"
    stage.rename(sdk)
    package = run / "source"
    shutil.copytree(
        source / "packages/gr_diagonal",
        package,
        ignore=shutil.ignore_patterns("__pycache__", "build", "*.egg-info"),
    )
    # Reject an absent dependency with package search restricted to an empty root.
    empty = run / "empty"
    empty.mkdir()
    missing = execute(
        [
            args.cmake,
            "-S",
            package,
            "-B",
            run / "missing",
            *generator,
            "-DCMAKE_FIND_ROOT_PATH=" + str(empty),
            "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY",
            "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
            "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
        ],
        expected_failure=True,
        env=environment,
    )
    missing_diagnostic = missing.stdout + missing.stderr
    if (
        'Could not find a package configuration file provided by "GR"'
        not in missing_diagnostic
        or "GRConfig.cmake" not in missing_diagnostic
    ):
        raise AssertionError("Missing-SDK probe failed for an unrelated reason")
    # Deliberately incompatible declaration against this actual installed SDK.
    # This checks CMake version rejection without requiring ignored old binaries.
    # It is not evidence of building against a historical SDK.
    wrong_source = run / "wrong-source"
    wrong_source.mkdir()
    (wrong_source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.24)\nproject(WrongSDK NONE)\nfind_package(GR 0.6.0 EXACT REQUIRED)\n",
        encoding="utf-8",
    )
    wrong = execute(
        [
            args.cmake,
            "-S",
            wrong_source,
            "-B",
            run / "wrong",
            *generator,
            "-DGR_DIR=" + str(sdk / "lib/cmake/GR"),
        ],
        expected_failure=True,
        env=environment,
    )
    wrong_diagnostic = wrong.stdout + wrong.stderr
    if (
        'configuration file for package "GR" that exactly matches'
        not in wrong_diagnostic
        or "version: 0.7.0" not in wrong_diagnostic
        or "0.6.0" not in wrong_diagnostic
    ):
        raise AssertionError("Wrong-version declaration failed for an unrelated reason")
    native_build = run / "b"
    execute(
        [
            args.cmake,
            "-S",
            package,
            "-B",
            native_build,
            *generator,
            *flags,
            "-DCMAKE_PREFIX_PATH=" + str(sdk),
        ],
        env=environment,
    )
    execute(
        [
            args.cmake,
            "--build",
            native_build,
            "--config",
            args.config,
            "--parallel",
            "4",
        ]
    )
    sanitizer = build / args.config / "clang_rt.asan_dynamic-x86_64.dll"
    if os.name == "nt" and "sanitize" in cached("CMAKE_CXX_FLAGS"):
        shutil.copy2(sanitizer, native_build / args.config / sanitizer.name)
    ctest = Path(args.cmake).with_name("ctest.exe" if os.name == "nt" else "ctest")
    execute(
        [ctest, "--test-dir", native_build, "-C", args.config, "--output-on-failure"],
        env=environment,
    )
    package_stage = run / "package-stage"
    execute(
        [
            args.cmake,
            "--install",
            native_build,
            "--config",
            args.config,
            "--prefix",
            package_stage,
        ]
    )
    installed = run / "package"
    package_stage.rename(installed)
    consumer = run / "consumer"
    consumer.mkdir()
    shutil.copy2(package / "tests/conformance.cpp", consumer / "main.cpp")
    (consumer / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.24)\nproject(PackageConsumer LANGUAGES CXX)\nfind_package(GRDiagonal 0.2.0 EXACT REQUIRED)\nadd_executable(consumer main.cpp)\ntarget_link_libraries(consumer PRIVATE GRDiagonal::diagonal)\n",
        encoding="utf-8",
    )
    consumer_build = run / "c"
    execute(
        [
            args.cmake,
            "-S",
            consumer,
            "-B",
            consumer_build,
            *generator,
            *flags,
            "-DCMAKE_PREFIX_PATH=" + str(installed) + ";" + str(sdk),
        ]
    )
    execute([args.cmake, "--build", consumer_build, "--config", args.config])
    if os.name == "nt" and "sanitize" in cached("CMAKE_CXX_FLAGS"):
        shutil.copy2(sanitizer, consumer_build / args.config / sanitizer.name)
        shutil.copy2(sanitizer, installed / "bin" / sanitizer.name)
    executable = next(
        path
        for path in [
            consumer_build / args.config / "consumer.exe",
            consumer_build / "consumer.exe",
            consumer_build / "consumer",
        ]
        if path.is_file()
    )
    execute([executable], env=environment)
    python = (
        source
        / "build/sdk-venv"
        / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    )
    if not python.is_file():
        python = Path(sys.executable)
    wheels = run / "wheels"
    execute(
        [
            python,
            "-m",
            "pip",
            "wheel",
            "--no-index",
            "--no-deps",
            "--no-build-isolation",
            "--no-cache-dir",
            "--wheel-dir",
            wheels,
            package / "python",
        ]
    )
    wheel = next(wheels.glob("gr_diagonal-0.2.0-*.whl"))
    destination = run / "python"
    execute(
        [
            python,
            "-m",
            "pip",
            "install",
            "--no-index",
            "--no-deps",
            "--no-cache-dir",
            "--target",
            destination,
            wheel,
        ]
    )
    library = next(
        iter(
            list((installed / "bin").glob("gr_diagonal_native.dll"))
            + list((installed / "lib").glob("libgr_diagonal_native.*"))
        )
    )
    environment["GR_DIAGONAL_LIBRARY"] = str(library)
    code = 'import sys,runpy; sys.path.insert(0,sys.argv[1]); path=sys.argv[2]; sys.argv=[path]; runpy.run_path(path,run_name="__main__")'
    execute(
        [python, "-I", "-B", "-c", code, destination, package / "tests/python_test.py"],
        env=environment,
        cwd=run,
    )
    (run / "artifacts.json").write_text(
        json.dumps(
            {
                "wheel": str(wheel),
                "wheel_sha256": hashlib.sha256(wheel.read_bytes()).hexdigest(),
                "native": str(library),
                "native_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
                "manifest": json.loads(
                    (installed / "share/gr_diagonal/manifest.json").read_text()
                ),
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print(
        "Independent source package: missing/wrong SDK rejected; standalone build/conformance, relocated C++ Function composition, and installed Python wheel/C facade passed"
    )
    print("Artifacts:", run / "artifacts.json")
finally:
    (run / "commands.json").write_text(json.dumps(events, indent=2), encoding="utf-8")
