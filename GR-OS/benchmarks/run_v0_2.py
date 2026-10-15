"""Measure v0.2-only reader/history/typed paths; no v0.1 equivalence claim."""
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
from run import document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    if not 3 <= args.repeats <= 30:
        parser.error("repeats must be 3..30")
    cases = []

    def measure(name, fn, work):
        fn()
        samples = []
        for _ in range(args.repeats):
            start = time.perf_counter_ns()
            fn()
            samples.append((time.perf_counter_ns() - start) / 1_000_000)
        median = statistics.median(samples)
        cases.append({"name": name, "work": work, "samples_ms": samples,
                      "median_ms": median, "minimum_ms": min(samples), "maximum_ms": max(samples)})
        print(f"{name}: {median:.3f} ms median", flush=True)

    material = b"abcd" * 1048576
    digest = "sha256:" + hashlib.sha256(material).hexdigest()
    with tempfile.TemporaryDirectory(prefix="gr-os-read-benchmark-") as temporary:
        for backend in ("memory", "sqlite"):
            options = {"backend": backend}
            if backend == "sqlite":
                options["path"] = Path(temporary) / "reader.sqlite"
            with gr_os.Storage(**options) as store:
                store.create("reader")
                store.commit("reader", [], expected_revision="0", transaction_id="binary",
                             actor="bench:recorder", reason="synthetic reader measurement",
                             blobs=[material], required_blob_digests=[digest])

                def drain(reader):
                    buffer = bytearray(65536)
                    offset = 0
                    while offset < len(material):
                        count = reader.readinto(buffer, offset)
                        assert count and buffer[:count] == material[offset:offset + count]
                        offset += count

                def open_verified():
                    with store.open_blob("reader", digest) as reader:
                        assert reader.metadata["byte_length"] == len(material)

                def open_and_read():
                    with store.open_blob("reader", digest) as reader:
                        drain(reader)

                work = {"bytes": len(material), "chunk_bytes": 65536, "transport": "binary C readinto"}
                measure("verified_open_4MiB_" + backend, open_verified, work)
                measure("open_and_read_4MiB_" + backend, open_and_read, work)
                with store.open_blob("reader", digest) as reader:
                    measure("pinned_read_4MiB_" + backend, lambda: drain(reader), work)

    history = {"format": "gr.history/1", "nodes": []}
    snapshot = {"revision": "0", "records": [gr_os.construct(document(i)) for i in range(16)]}
    tips = []
    for step in range(24):
        if tips:
            snapshot = gr_os.history_reconstruct(history, tips[(step * 7) % len(tips)])
            parents = [snapshot["revision"]]
        else:
            parents = []
        changed = document(step % 16)
        changed["payload"]["step"] = step
        snapshot = gr_os.apply(snapshot, [gr_os.construct(changed)])
        history = gr_os.history_append(history, snapshot, parents,
                    {"agent": "bench:recorder", "method": "synthetic checkpoint", "reason": f"step {step}"})
        tips.append(history["nodes"][-1]["version"])
    history_work = {"nodes": 24, "current_per_checkpoint": 16,
                    "json_bytes": len(json.dumps(history, separators=(",", ":")).encode())}

    def validate():
        assert gr_os.history_validate(history)["valid"]

    def reconstruct():
        assert len(gr_os.history_reconstruct(history, tips[-1])["records"]) == 16

    measure("history_validate_24_checkpoints", validate, history_work)
    measure("history_reconstruct_24_checkpoints", reconstruct, history_work)

    inputs = [[gr_os.Value.bytes(b"x")]] * 1024
    for backend in ("cpu.sequential", "cpu.threaded"):
        def invoke(backend=backend):
            result = gr_os.execute_typed("gr.bytes.identity@1", "gr.cpu.identity.copy@1",
                                        inputs, backend=backend, max_threads=4)
            assert len(result.outputs) == 1024 and all(row[0].data == b"x" for row in result.outputs)
        measure("typed_identity_1024_" + backend, invoke, {"items": 1024, "input_bytes": 1024, "max_threads": 4})

    library = Path(os.environ["GR_OS_LIBRARY"]).resolve()
    result = {"format": "gr.benchmark/1", "suite": "public-sdk-v0.2/1", "label": "v0.2",
              "environment": {"platform": platform.platform(), "machine": platform.machine(),
                              "python": platform.python_version(), "cpu_count": os.cpu_count(),
                              "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest()},
              "method": {"clock": "perf_counter_ns", "warmups": 1, "repeats": args.repeats,
                         "includes": "Python/C ABI, validation and content assertions; warm temporary stores"},
              "cases": cases}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
