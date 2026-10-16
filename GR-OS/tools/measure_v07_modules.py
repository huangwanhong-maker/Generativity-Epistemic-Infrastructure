"""Measure accepted Release packages serially after all builds/tests stop."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-run", type=Path, required=True)
    parser.add_argument("--label", default="final")
    args = parser.parse_args()
    if not args.label.replace("-", "").isalnum():
        parser.error("label must contain only letters, digits and hyphens")
    root = Path(__file__).resolve().parents[1]
    run = args.package_run.resolve()
    if not run.is_relative_to(root):
        raise ValueError("Use an accepted package run inside GR-OS")
    artifacts = json.loads((run / "artifacts.json").read_text(encoding="utf-8"))
    if artifacts["sanitizer"]:
        raise ValueError("Performance captures require Release without ASan")
    for item in [*artifacts["libraries"].values(), *artifacts["wheels"]]:
        if digest(item["path"]) != item["sha256"]:
            raise ValueError(f"Accepted artifact changed: {item['path']}")
    evidence = root / "docs/validation"
    transcript = evidence / f"benchmark-v0.7-modules-commands-{args.label}.json"
    fixture = run / f"benchmark-fixtures-{args.label}"
    if transcript.exists() or fixture.exists():
        raise FileExistsError("Module capture or fixture directory already exists")
    records = []
    env = os.environ.copy()
    env.update({key: item["path"] for key, item in artifacts["libraries"].items()})
    launcher = 'import sys,runpy;sys.path.insert(0,sys.argv[1]);p=sys.argv[2];sys.argv=[p,*sys.argv[3:]];runpy.run_path(p,run_name="__main__")'

    def execute(name, command, stdout_json=True):
        output = evidence / f"benchmark-v0.7-{name}-{args.label}.json"
        log = output.with_suffix(".txt")
        if output.exists() or log.exists():
            raise FileExistsError(output)
        command = [str(value) for value in command]
        if not stdout_json:
            command += ["--output", str(output)]
        print("Running", name, flush=True)
        start = time.monotonic()
        record = {"name": name, "command": command,
                  "started_utc": datetime.now(timezone.utc).isoformat(),
                  "command_file_sha256": {p: digest(p) for p in command
                                          if Path(p).is_absolute() and Path(p).is_file()},
                  "native_libraries": artifacts["libraries"]}
        result = subprocess.run(command, cwd=root, env=env, capture_output=True)
        record.update(elapsed_s=time.monotonic() - start, returncode=result.returncode)
        records.append(record)
        log.write_bytes(result.stdout + b"\n[stderr]\n" + result.stderr)
        result.check_returncode()
        if stdout_json:
            output.write_bytes(result.stdout)
        json.loads(output.read_text(encoding="utf-8-sig"))
        record["capture_sha256"] = digest(output)
        print("Passed", name, f"({record['elapsed_s']:.2f}s)", flush=True)

    try:
        execute("analysis-native", [run / "ab/Release/benchmark_analysis.exe", "3", fixture])
        execute("visualization-native", [run / "vb/Release/benchmark_visualization.exe"])
        execute("modules-python", [sys.executable, "-I", "-B", "-c", launcher,
                artifacts["python"], run / "as/benchmark.py", "--fixtures", fixture,
                "--repeats", "5"], stdout_json=False)
    finally:
        transcript.write_text(json.dumps({"format": "gr.module-benchmark-commands/1",
            "package_run": str(run), "package_artifacts_sha256": digest(run / "artifacts.json"),
            "commands": records,
            "fixtures": [{"path": str(p), "bytes": p.stat().st_size, "sha256": digest(p)}
                         for p in sorted(fixture.glob("*")) if p.is_file()]}, indent=2) + "\n",
            encoding="utf-8")


if __name__ == "__main__":
    main()
