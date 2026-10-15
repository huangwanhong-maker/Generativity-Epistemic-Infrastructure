"""Bounded public-SDK benchmarks; writes measurements, never claims universal speed.

Run from any directory with GR_OS_LIBRARY/PYTHONPATH selecting the build to test.
Identical cases work with v0.1 and v0.2. Temporary data is synthetic and discarded.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import tempfile
import time

import gr_os


def document(index):
    return {"format": "gr.record/1", "id": f"bench:{index}", "type": "gr.assertion@1",
            "payload": {"index": index, "claim": "synthetic measurement fixture"},
            "attribution": {"agent": "bench:recorder", "method": "synthetic"}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    if not 3 <= args.repeats <= 30:
        parser.error("repeats must be 3..30")
    cases = []

    def measure(name, fn, work):
        fn()  # Warm library/loading paths before measurement.
        samples = []
        for _ in range(args.repeats):
            start = time.perf_counter_ns()
            fn()
            samples.append((time.perf_counter_ns() - start) / 1_000_000)
        result = {"name": name, "work": work, "samples_ms": samples,
                  "median_ms": statistics.median(samples), "minimum_ms": min(samples),
                  "maximum_ms": max(samples)}
        cases.append(result)
        print(f"{name}: {result['median_ms']:.3f} ms median", flush=True)

    records = [gr_os.construct(document(i)) for i in range(300)]
    snapshot = {"revision": "0", "records": records}
    measure("construct_100", lambda: [gr_os.construct(document(i)) for i in range(100)], {"records": 100})
    measure("validate_300", lambda: gr_os.validate(snapshot), {"records": 300})
    measure("graph_300", lambda: gr_os.graph(snapshot), {"records": 300})
    replacements = [gr_os.construct({**document(i), "payload": {"index": i, "revision": 2}}) for i in range(100)]
    measure("apply_100_of_300", lambda: gr_os.apply(snapshot, replacements), {"current": 300, "updates": 100})
    inputs = [bytes([i]) * 65536 for i in range(16)]
    expected = [hashlib.sha256(b).hexdigest().encode() for b in inputs]
    for backend in ["cpu.sequential", "cpu.threaded"]:
        def run(backend=backend):
            result = gr_os.execute("sha256", inputs, backend=backend, max_threads=4)
            assert result.outputs == expected
        measure("sha256_1MiB_" + backend, run, {"items": 16, "input_bytes": 1048576, "max_threads": 4})

    with tempfile.TemporaryDirectory(prefix="gr-os-benchmark-") as temporary:
        for backend in ["memory", "sqlite"]:
            index = 0
            options = {"backend": backend}
            if backend == "sqlite":
                options["path"] = Path(temporary) / "bench.sqlite"
            with gr_os.Storage(**options) as store:
                def commit():
                    nonlocal index
                    workspace = f"work:{index}"
                    index += 1
                    store.create(workspace)
                    result = store.commit(workspace, records[:100], expected_revision="0", transaction_id="first",
                                          actor="bench:recorder", reason="synthetic benchmark")
                    assert store.head(workspace) == result["revision"]
                measure("commit_100_" + backend, commit, {"records": 100, "includes_workspace_creation": True})
                material = b"abcd" * 1048576
                digest = "sha256:" + hashlib.sha256(material).hexdigest()
                artifact = document(10000)
                artifact["artifacts"] = [{"sha256": digest, "byte_length": len(material), "media_type": "application/octet-stream"}]
                store.create("binary")
                store.commit("binary", [gr_os.construct(artifact)], expected_revision="0", transaction_id="binary",
                             actor="bench:recorder", reason="synthetic benchmark", blobs=[material])
                def read():
                    assert store.blob("binary", digest) == material
                measure("blob_4MiB_" + backend, read, {"bytes": len(material), "transport": "v1 base64 convenience"})

    library = Path(os.environ["GR_OS_LIBRARY"]).resolve()
    result = {"format": "gr.benchmark/1", "suite": "public-sdk-common/1", "label": args.label,
              "environment": {"platform": platform.platform(), "machine": platform.machine(),
                              "python": platform.python_version(), "cpu_count": os.cpu_count(),
                              "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest()},
              "method": {"clock": "perf_counter_ns", "warmups": 1, "repeats": args.repeats,
                         "includes": "Python/C ABI transport, validation and native work; no isolated kernel speed claim"},
              "cases": cases}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
