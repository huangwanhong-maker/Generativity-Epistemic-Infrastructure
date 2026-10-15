"""Real process termination at deterministic SQLite publication barriers.

This is process-crash/restart evidence, not a simulated power-loss test.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import time


def invoke(worker: Path, mode: str, database: Path) -> dict:
    completed = subprocess.run(
        [str(worker), mode, str(database)], capture_output=True, text=True,
        encoding="utf-8", timeout=30, check=False,
    )
    if completed.returncode:
        raise AssertionError(f"{mode}: {completed.returncode}: {completed.stderr}")
    return json.loads(completed.stdout)


def assert_inventory(result: dict, committed: bool) -> None:
    assert result["quick_check"] == "ok", result
    assert result["receipt"] is committed and result["history"] is committed, result
    assert result["records"] == int(committed), result
    assert (result["head"] != "0") is committed, result
    # One user artifact plus compact state payload, node header and catalogue.
    expected = {"revisions": 1, "record_versions": 2, "snapshot_records": 2,
                "blobs": 4, "revision_blobs": 4} if committed else {
                    "revisions": 0, "record_versions": 0, "snapshot_records": 0,
                    "blobs": 0, "revision_blobs": 0}
    assert result["counts"] == expected, result


def exercise(worker: Path, directory: Path, phase: str) -> dict:
    database = directory / f"{phase}.sqlite"
    marker = directory / f"{phase}.ready"
    invoke(worker, "prepare", database)
    child = subprocess.Popen(
        [str(worker), "run", str(database), str(marker), phase],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8",
    )
    try:
        deadline = time.monotonic() + 30
        while not marker.exists():
            if child.poll() is not None:
                stdout, stderr = child.communicate(timeout=2)
                raise AssertionError(f"{phase}: worker exited before barrier: {stdout} {stderr}")
            if time.monotonic() >= deadline:
                raise AssertionError(f"{phase}: worker failed to reach publication barrier")
            time.sleep(0.01)
        # TerminateProcess on Windows; SIGKILL on POSIX. No destructor or RAII
        # rollback executes in the killed process.
        child.kill()
        child.communicate(timeout=10)
        assert child.returncode != 0, "Worker unexpectedly exited normally"
    finally:
        if child.poll() is None:
            child.kill()
            child.communicate(timeout=10)
    observed = invoke(worker, "inspect", database)
    assert_inventory(observed, committed=phase == "after")
    first = invoke(worker, "retry", database)
    assert first["state"] == "committed", first
    assert first["publication"]["replayed"] is (phase == "after"), first
    second = invoke(worker, "retry", database)
    assert second["publication"]["replayed"] is True, second
    assert first["request_digest"] == second["request_digest"], (first, second)
    assert first["publication"]["revision"] == second["publication"]["revision"], (first, second)
    assert_inventory(invoke(worker, "inspect", database), committed=True)
    return {"phase": phase, "process_terminated": True,
            "committed_on_reopen": phase == "after", "exact_retry": True}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="gr-os-runtime-crash-") as directory:
        results = [exercise(args.worker.resolve(), Path(directory), phase)
                   for phase in ("before", "during", "after")]
    print(json.dumps({"process_crash_recovery": results, "power_loss_tested": False}))


if __name__ == "__main__":
    main()
