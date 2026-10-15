"""Run serial v0.5/v0.6 measurements after build and test work has stopped.

Requires the preserved v0.5 baseline, current Release benchmarks, NumPy in the
selected interpreter, and a successful independent_package installation. No
timing thresholds. Existing evidence is never overwritten.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-run", required=True, type=Path)
    parser.add_argument("--label", default="final")
    args = parser.parse_args()
    if not args.label.replace("-", "").isalnum():
        parser.error("label must contain only letters, digits and hyphens")
    root = Path(__file__).resolve().parents[1]
    evidence = root / "docs/validation"
    baseline = root / "build/baselines/v0.5"
    release = root / "build/Release"
    package = args.package_run.resolve()
    artifact = json.loads((package / "artifacts.json").read_text(encoding="utf-8"))
    import numpy

    print("NumPy", numpy.__version__, "available", flush=True)
    for key in ("native", "wheel"):
        actual = hashlib.sha256(Path(artifact[key]).read_bytes()).hexdigest()
        if actual != artifact[key + "_sha256"]:
            raise ValueError(f"Independent package {key} differs from accepted artifacts")
    transcript = evidence / f"benchmark-v0.6-commands-{args.label}.json"
    if transcript.exists():
        raise FileExistsError(transcript)
    events = []

    def run(stem, command, *, stdout_json=False, env=None):
        output = evidence / f"{stem}-{args.label}.json"
        log = output.with_suffix(".txt")
        if output.exists() or log.exists():
            raise FileExistsError(output)
        command = [str(value) for value in command]
        if not stdout_json:
            command.append(str(output))
        print("Running", stem, flush=True)
        started = datetime.now(timezone.utc).isoformat()
        start = time.monotonic()
        result = subprocess.run(command, cwd=root, env=env, capture_output=True)
        elapsed = time.monotonic() - start
        log.write_bytes(result.stdout + b"\n[stderr]\n" + result.stderr)
        event = {"name": stem, "command": command, "started_utc": started,
                 "elapsed_s": elapsed, "returncode": result.returncode,
                 "command_file_sha256": {value: hashlib.sha256(Path(value).read_bytes()).hexdigest()
                     for value in command if value != str(output) and Path(value).is_file()},
                 "environment_overrides": {key: env[key] for key in
                     ("PYTHONPATH", "GR_OS_LIBRARY", "GR_DIAGONAL_LIBRARY")
                     if env and key in env}}
        events.append(event)
        transcript.write_text(json.dumps(events, indent=2) + "\n", encoding="utf-8")
        result.check_returncode()
        if stdout_json:
            output.write_bytes(result.stdout)
        json.loads(output.read_text(encoding="utf-8-sig"))
        event["capture_sha256"] = hashlib.sha256(output.read_bytes()).hexdigest()
        print("Passed", stem, f"({elapsed:.2f}s)", flush=True)

    try:
        # First pair old/new, second new/old to expose order and host variation.
        for version, directory, pair in (("v0.5", baseline, 1), ("v0.6", release, 1),
                                         ("v0.6", release, 2), ("v0.5", baseline, 2)):
            suffix = "-for-v0.6" if version == "v0.5" else ""
            run(f"benchmark-{version}-data{suffix}-pair{pair}", [directory / "benchmark_data.exe"])
        for version, directory in (("v0.5", baseline), ("v0.6", release)):
            suffix = "-for-v0.6" if version == "v0.5" else ""
            run(f"benchmark-{version}-relational{suffix}",
                [directory / "benchmark_data_relational.exe"], stdout_json=True)
        for version, directory in (("v0.5", baseline), ("v0.6", release)):
            suffix = "-for-v0.6" if version == "v0.5" else ""
            run(f"benchmark-{version}-runtime{suffix}", [directory / "benchmark_runtime.exe"])
        for version in ("v0.5", "v0.6"):
            env = os.environ.copy()
            old = version == "v0.5"
            env["PYTHONPATH"] = str(baseline / "python" if old else root / "bindings/python/src")
            env["GR_OS_LIBRARY"] = str(baseline / "installed/bin/gr_native.dll" if old else release / "gr_native.dll")
            runner = baseline / "benchmarks/data.py" if old else root / "benchmarks/data.py"
            suffix = "-for-v0.6" if old else ""
            run(f"benchmark-{version}-python{suffix}", [sys.executable, "-B", runner, "--output"], env=env)
        run("benchmark-v0.6-access", [release / "benchmark_access.exe"])
        run("benchmark-v0.6-language-profile", [release / "benchmark_data_language_profile.exe"])
        run("benchmark-v0.6-relational-profile", [release / "benchmark_data_relational_profile.exe"], stdout_json=True)
        env = os.environ.copy()
        env["PYTHONPATH"] = str(package / "python")
        env["GR_DIAGONAL_LIBRARY"] = artifact["native"]
        run("benchmark-v0.6-independent-package",
            [sys.executable, "-B", root / "packages/gr_diagonal/benchmark.py"], env=env)
    finally:
        transcript.write_text(json.dumps(events, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
