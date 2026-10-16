"""Serial unchanged-workload v0.6/v0.7 measurements. No timing pass/fail threshold.

Use after builds/tests have stopped. Requires preserved v0.6 executables/facade,
current Release artifacts and NumPy in the selected interpreter. New module
benchmarks are run separately against the accepted installed package prefix.
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
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="final")
    parser.add_argument("--skip-python", action="store_true")
    args = parser.parse_args()
    if not args.label.replace("-", "").isalnum():
        parser.error("label must contain only letters, digits and hyphens")
    root = Path(__file__).resolve().parents[1]
    evidence = root / "docs/validation"
    baseline = root / "build/baselines/v0.6"
    release = root / "build/Release"
    transcript = evidence / f"benchmark-v0.7-commands-{args.label}.json"
    if transcript.exists():
        raise FileExistsError(transcript)
    # Compare source bytes, not only case labels, before claiming unchanged work.
    archive = zipfile.ZipFile(baseline / "source.zip")
    runners = ["benchmarks/data.cpp", "benchmarks/data_relational.cpp",
               "benchmarks/runtime.cpp", "benchmarks/data.py"]
    for name in runners:
        if archive.read(name) != (root / name).read_bytes():
            raise ValueError(f"matched workload source changed: {name}")
    if not args.skip_python:
        import numpy
        print("NumPy", numpy.__version__, flush=True)
    events = []

    def run(name, command, *, stdout_json=False, env=None):
        output = evidence / f"{name}-{args.label}.json"
        log = output.with_suffix(".txt")
        if output.exists() or log.exists():
            raise FileExistsError(output)
        command = [str(value) for value in command]
        if not stdout_json:
            command.append(str(output))
        print("Running", name, flush=True)
        start = time.monotonic()
        started = datetime.now(timezone.utc).isoformat()
        result = subprocess.run(command, cwd=root, env=env, capture_output=True)
        log.write_bytes(result.stdout + b"\n[stderr]\n" + result.stderr)
        event = {"name": name, "command": command, "started_utc": started,
                 "elapsed_s": time.monotonic() - start, "returncode": result.returncode,
                 "command_file_sha256": {value: hashlib.sha256(Path(value).read_bytes()).hexdigest()
                     for value in command if value != str(output) and Path(value).is_file()},
                 "environment_overrides": {key: env[key] for key in
                     ("PYTHONPATH", "GR_OS_LIBRARY") if env and key in env},
                 "native_library_sha256": hashlib.sha256(Path(env["GR_OS_LIBRARY"]).read_bytes()).hexdigest()
                     if env and "GR_OS_LIBRARY" in env else None}
        events.append(event)
        transcript.write_text(json.dumps(events, indent=2) + "\n", encoding="utf-8")
        result.check_returncode()
        if stdout_json:
            output.write_bytes(result.stdout)
        json.loads(output.read_text(encoding="utf-8-sig"))
        event["capture_sha256"] = hashlib.sha256(output.read_bytes()).hexdigest()
        print("Passed", name, f"({event['elapsed_s']:.2f}s)", flush=True)

    try:
        for version, directory, pair in (("v0.6", baseline, 1), ("v0.7", release, 1),
                                         ("v0.7", release, 2), ("v0.6", baseline, 2)):
            suffix = "-for-v0.7" if version == "v0.6" else ""
            run(f"benchmark-{version}-data{suffix}-pair{pair}", [directory / "benchmark_data.exe"])
        for version, directory in (("v0.6", baseline), ("v0.7", release)):
            suffix = "-for-v0.7" if version == "v0.6" else ""
            run(f"benchmark-{version}-relational{suffix}",
                [directory / "benchmark_data_relational.exe"], stdout_json=True)
            run(f"benchmark-{version}-runtime{suffix}", [directory / "benchmark_runtime.exe"])
        if not args.skip_python:
            for version in ("v0.6", "v0.7"):
                old = version == "v0.6"
                env = os.environ.copy()
                env["PYTHONPATH"] = str(baseline / "python" if old else root / "bindings/python/src")
                env["GR_OS_LIBRARY"] = str(baseline / "installed/bin/gr_native.dll" if old else release / "gr_native.dll")
                runner = baseline / "benchmarks/data.py" if old else root / "benchmarks/data.py"
                suffix = "-for-v0.7" if old else ""
                run(f"benchmark-{version}-python{suffix}", [sys.executable, "-B", runner, "--output"], env=env)
    finally:
        transcript.write_text(json.dumps(events, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
