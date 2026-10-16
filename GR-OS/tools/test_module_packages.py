"""Standalone source builds, relocation and composition without old build artifacts.

Prerequisites are the current built SDK, CMake/compiler, and local offline wheel
build tools. Every native package is copied outside the central source tree and
finds only installed dependencies. No network or global installation is used.
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
parser.add_argument(
    "--older-sdk", type=Path, help="Optional actual historical SDK rejection probe"
)
args = parser.parse_args()
build, source = args.build.resolve(), args.source.resolve()
run = build / "m" / uuid.uuid4().hex[:8]
run.mkdir(parents=True)
cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
events = []


def cached(name):
    return next(
        (v.split("=", 1)[1] for v in cache.splitlines() if v.startswith(name + ":")), ""
    )


def execute(command, *, expected_failure=False, **kwargs):
    command = [str(v) for v in command]
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
    (run / "commands.json").write_text(json.dumps(events, indent=2), encoding="utf-8")
    if expected_failure:
        if not result.returncode:
            raise AssertionError("Negative dependency probe unexpectedly succeeded")
    elif result.returncode:
        print(result.stdout)
        print(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, command)
    return result.stdout + result.stderr


def copytree(origin, destination):
    shutil.copytree(
        origin,
        destination,
        ignore=shutil.ignore_patterns("__pycache__", "*.egg-info", "build", "dist"),
    )


generator = ["-G", cached("CMAKE_GENERATOR")] if cached("CMAKE_GENERATOR") else []
if cached("CMAKE_GENERATOR_PLATFORM"):
    generator += ["-A", cached("CMAKE_GENERATOR_PLATFORM")]
flags = ["-DCMAKE_BUILD_TYPE=" + args.config]
asan = "sanitize" in cached("CMAKE_CXX_FLAGS")
for name in (
    "CMAKE_C_FLAGS",
    "CMAKE_CXX_FLAGS",
    "CMAKE_EXE_LINKER_FLAGS",
    "CMAKE_SHARED_LINKER_FLAGS",
):
    if "sanitize" in cached(name) or ("LINKER" in name and asan):
        flags.append("-D" + name + "=" + cached(name))
environment = os.environ.copy()
for name in (
    "CMAKE_PREFIX_PATH",
    "GR_DIR",
    "GRRelationalAnalysis_DIR",
    "GRVisualization_DIR",
    "GRDiagonal_DIR",
    "PYTHONPATH",
):
    environment.pop(name, None)
environment["PYTHONDONTWRITEBYTECODE"] = "1"
sanitizer = (
    build / args.config / "clang_rt.asan_dynamic-x86_64.dll"
    if os.name == "nt" and asan
    else None
)
ctest = Path(args.cmake).with_name("ctest.exe" if os.name == "nt" else "ctest")


def stage_runtime(directory):
    if sanitizer is not None:
        if not sanitizer.is_file():
            raise RuntimeError(
                "Stage the installed ASan runtime beside the main build DLL first"
            )
        directory.mkdir(parents=True, exist_ok=True)
        shutil.copy2(sanitizer, directory / sanitizer.name)


def executable(directory, name):
    return next(
        p
        for p in [
            directory / args.config / (name + ".exe"),
            directory / (name + ".exe"),
            directory / name,
        ]
        if p.is_file()
    )


def native(prefix, name):
    return next(
        iter(
            list((prefix / "bin").glob(name + ".dll"))
            + list((prefix / "lib").glob("lib" + name + ".so"))
            + list((prefix / "lib").glob("lib" + name + ".dylib"))
        )
    )


try:
    sdk_stage = run / "sdk-stage"
    execute(
        [args.cmake, "--install", build, "--config", args.config, "--prefix", sdk_stage]
    )
    sdk = run / "sdk"
    sdk_stage.rename(sdk)
    stage_runtime(sdk / "bin")
    prefixes = [sdk]
    tags = {"gr_relational_analysis": "a", "gr_visualization": "v", "gr_diagonal": "d"}
    packages = {}
    installed = {}
    for name in ("gr_relational_analysis", "gr_visualization", "gr_diagonal"):
        tag = tags[name]
        package = run / (tag + "s")
        copytree(source / "packages" / name, package)
        packages[name] = package
        # A completely empty package search root establishes a real missing SDK.
        empty = run / (tag + "e")
        empty.mkdir()
        diagnostic = execute(
            [
                args.cmake,
                "-S",
                package,
                "-B",
                run / (tag + "m"),
                *generator,
                "-DCMAKE_FIND_ROOT_PATH=" + str(empty),
                "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY",
                "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
            ],
            expected_failure=True,
            env=environment,
        )
        if (
            'Could not find a package configuration file provided by "GR"'
            not in diagnostic
            or "GRConfig.cmake" not in diagnostic
        ):
            raise AssertionError("Missing SDK probe failed for an unrelated reason")
        if args.older_sdk is not None:
            diagnostic = execute(
                [
                    args.cmake,
                    "-S",
                    package,
                    "-B",
                    run / (tag + "h"),
                    *generator,
                    "-DGR_DIR=" + str(args.older_sdk.resolve() / "lib/cmake/GR"),
                ],
                expected_failure=True,
                env=environment,
            )
            if (
                'configuration file for package "GR" that exactly matches'
                not in diagnostic
                or "0.7.0" not in diagnostic
            ):
                raise AssertionError(
                    "Historical SDK rejection failed for an unrelated reason"
                )
        if name == "gr_visualization":
            diagnostic = execute(
                [
                    args.cmake,
                    "-S",
                    package,
                    "-B",
                    run / "vma",
                    *generator,
                    "-DGR_DIR=" + str(sdk / "lib/cmake/GR"),
                    "-DCMAKE_PREFIX_PATH=" + str(sdk),
                    "-DCMAKE_FIND_ROOT_PATH=" + str(sdk),
                    "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY",
                    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
                    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
                ],
                expected_failure=True,
                env=environment,
            )
            if (
                'Could not find a package configuration file provided by "GRRelationalAnalysis"'
                not in " ".join(diagnostic.split())
            ):
                raise AssertionError(
                    "Missing analysis dependency failed for an unrelated reason"
                )
        native_build = run / (tags[name] + "b")
        execute(
            [
                args.cmake,
                "-S",
                package,
                "-B",
                native_build,
                *generator,
                *flags,
                "-DCMAKE_PREFIX_PATH=" + ";".join(map(str, prefixes)),
                "-DGR_MODULE_BUILD_BENCHMARKS=ON",
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
            ],
            env=environment,
        )
        stage_runtime(native_build / args.config)
        execute(
            [
                ctest,
                "--test-dir",
                native_build,
                "-C",
                args.config,
                "--output-on-failure",
            ],
            env=environment,
        )
        package_stage = run / (tag + "t")
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
        package_prefix = run / tag
        package_stage.rename(package_prefix)
        stage_runtime(package_prefix / "bin")
        installed[name] = package_prefix
        prefixes.append(package_prefix)

    # Exact-version behavior is tested against actual newly installed configs.
    # This intentionally incompatible declaration is not an old-SDK rebuild.
    wrong = run / "wd"
    wrong.mkdir()
    for package, version in (
        ("GR", "0.6.0"),
        ("GRRelationalAnalysis", "0.2.0"),
        ("GRVisualization", "0.2.0"),
    ):
        (wrong / "CMakeLists.txt").write_text(
            f"cmake_minimum_required(VERSION 3.24)\nproject(WrongDeclaration NONE)\nfind_package({package} {version} EXACT REQUIRED)\n",
            encoding="utf-8",
        )
        diagnostic = execute(
            [
                args.cmake,
                "-S",
                wrong,
                "-B",
                run / (package + "-wrong"),
                *generator,
                "-DCMAKE_PREFIX_PATH=" + ";".join(map(str, prefixes)),
            ],
            expected_failure=True,
            env=environment,
        )
        if (
            f'configuration file for package "{package}" that exactly matches'
            not in " ".join(diagnostic.split())
            or version not in diagnostic
        ):
            raise AssertionError("Wrong-version probe failed for an unrelated reason")

    consumer_source = run / "cs"
    copytree(source / "tests/integration/modules", consumer_source)
    shutil.copy2(
        source / "examples/python/modules.py", consumer_source / "python_example.py"
    )
    consumer_build = run / "c"
    execute(
        [
            args.cmake,
            "-S",
            consumer_source,
            "-B",
            consumer_build,
            *generator,
            *flags,
            "-DCMAKE_PREFIX_PATH=" + ";".join(map(str, prefixes)),
        ],
        env=environment,
    )
    execute(
        [
            args.cmake,
            "--build",
            consumer_build,
            "--config",
            args.config,
            "--parallel",
            "4",
        ],
        env=environment,
    )
    consumer = executable(consumer_build, "consumer_modules")
    stage_runtime(consumer.parent)
    fixture = run / "fixture"
    execute([consumer, fixture], env=environment)
    for name, stem in (
        ("gr_relational_analysis", "analysis"),
        ("gr_visualization", "visualization"),
    ):
        demo = executable(run / (tags[name] + "b"), "example_" + stem)
        command = [demo, run / "visual-demo"] if stem == "visualization" else [demo]
        execute(command, env=environment, cwd=run)

    tooling = (
        source
        / "build/sdk-venv"
        / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    )
    if not tooling.is_file():
        tooling = Path(sys.executable)
    execute([tooling, "-c", "import setuptools,wheel"], env=environment)
    core_python = run / "ps"
    copytree(source / "bindings/python", core_python)
    wheels, destination = run / "wheels", run / "python"
    wheel_files = []
    for location in [core_python, *(p / "python" for p in packages.values())]:
        execute(
            [
                tooling,
                "-m",
                "pip",
                "wheel",
                "--disable-pip-version-check",
                "--no-index",
                "--no-deps",
                "--no-build-isolation",
                "--no-cache-dir",
                "--wheel-dir",
                wheels,
                location,
            ],
            env=environment,
        )
    wheel_files = sorted(wheels.glob("*.whl"))
    execute(
        [
            tooling,
            "-m",
            "pip",
            "install",
            "--disable-pip-version-check",
            "--no-index",
            "--no-deps",
            "--no-cache-dir",
            "--target",
            destination,
            *wheel_files,
        ],
        env=environment,
    )
    libraries = {
        "GR_OS_LIBRARY": native(sdk, "gr_native"),
        "GR_ANALYSIS_LIBRARY": native(
            installed["gr_relational_analysis"], "gr_relational_analysis_native"
        ),
        "GR_VISUALIZATION_LIBRARY": native(
            installed["gr_visualization"], "gr_visualization_native"
        ),
        "GR_DIAGONAL_LIBRARY": native(installed["gr_diagonal"], "gr_diagonal_native"),
    }
    environment.update({key: str(value) for key, value in libraries.items()})
    environment["GR_MODULE_FIXTURE"] = str(fixture)
    code = 'import sys,runpy;sys.path.insert(0,sys.argv[1]);p=sys.argv[2];sys.argv=[p,*sys.argv[3:]];runpy.run_path(p,run_name="__main__")'
    execute(
        [
            sys.executable,
            "-I",
            "-B",
            "-c",
            code,
            destination,
            consumer_source / "python_consumer.py",
        ],
        env=environment,
        cwd=run,
    )
    execute(
        [
            sys.executable,
            "-I",
            "-B",
            "-c",
            code,
            destination,
            consumer_source / "python_example.py",
            "--fixture",
            fixture,
            "--output",
            run / "python-demo",
        ],
        env=environment,
        cwd=run,
    )
    artifacts = {
        "sdk": str(sdk),
        "prefixes": {k: str(v) for k, v in installed.items()},
        "python": str(destination),
        "fixture": str(fixture),
        "sanitizer": asan,
        "historical_sdk_probe": str(args.older_sdk) if args.older_sdk else None,
        "wheels": [
            {"path": str(p), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
            for p in wheel_files
        ],
        "libraries": {
            key: {"path": str(p), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
            for key, p in libraries.items()
        },
    }
    (run / "artifacts.json").write_text(
        json.dumps(artifacts, indent=2), encoding="utf-8"
    )
    print(
        "Independent installed packages and C++/Python composition passed:",
        run / "artifacts.json",
    )
finally:
    (run / "commands.json").write_text(json.dumps(events, indent=2), encoding="utf-8")
