"""Owned Python/native transfer costs; no v0.3 equivalent is claimed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import time

import gr_os


def measure(operation, repeats):
    operation()
    samples = []
    for _ in range(repeats):
        start = time.perf_counter_ns()
        operation()
        samples.append((time.perf_counter_ns() - start) / 1e6)
    return {"samples_ms": samples, "median_ms": statistics.median(samples)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    if not 3 <= args.repeats <= 20:
        parser.error("repeats must be 3..20")
    native = gr_os.Native()
    rows = []
    for size in (1024, 1024 * 1024, 4 * 1024 * 1024):
        source = bytearray((i % 251 for i in range(size)))
        expected = hashlib.sha256(source).hexdigest()
        destination = bytearray(size)

        def copy_in():
            with gr_os.Buffer(source, native=native):
                pass

        row = {"payload_bytes": size, "copy_import": measure(copy_in, args.repeats)}
        with gr_os.Buffer(source, native=native) as value:
            def copy_out():
                if value.readinto(destination) != size:
                    raise AssertionError("Incomplete copy")

            def exported_views():
                for _ in range(1000):
                    view = value.memoryview()
                    view.release()

            row["copy_readinto"] = measure(copy_out, args.repeats)
            row["readonly_export_1000"] = measure(exported_views, args.repeats)
            view = value.memoryview()
            assert view.readonly and hashlib.sha256(view).hexdigest() == expected
            assert hashlib.sha256(destination).hexdigest() == expected
        source[:] = bytes(size)
        # Native handle and writable import source no longer own valid contents;
        # the exported view alone pins its independent immutable allocation.
        assert hashlib.sha256(view).hexdigest() == expected
        view.release()
        row["verification"] = "exact SHA-256; readonly export survives handle close and input mutation"
        rows.append(row)
    result = {
        "suite": "gr.buffer.interop/1", "platform": platform.platform(),
        "python": platform.python_version(), "repeats": args.repeats,
        "native_sha256": hashlib.sha256(Path(os.environ["GR_OS_LIBRARY"]).read_bytes()).hexdigest(),
        "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "method": "one warmup per operation; complete create/close or pin/release lifetimes timed; hashes outside timing",
        "limitations": "New v0.4 API without a v0.3 counterpart. Copy-in includes Python staging and native allocation; export measures ownership/metadata transport, not numeric computation. No NumPy dependency.",
        "cases": rows,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
