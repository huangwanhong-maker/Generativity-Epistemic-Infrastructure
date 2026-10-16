"""Interleaved diagnostic of the installed 768-owner Python byte-copy boundary."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-run", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.package_run.resolve()
    artifacts = json.loads((run / "artifacts.json").read_text())
    if artifacts["sanitizer"]:
        raise ValueError("Use accepted Release artifacts")
    output = root / "docs/validation/benchmark-v0.7-python-bridge-profile.json"
    if output.exists():
        raise FileExistsError(output)
    sys.path.insert(0, artifacts["python"])
    for key, item in artifacts["libraries"].items():
        if hashlib.sha256(Path(item["path"]).read_bytes()).hexdigest() != item["sha256"]:
            raise ValueError("Accepted native artifact changed")
        os.environ[key] = item["path"]
    from gr_os.data import Context, ValueSpec
    from gr_os.execution import Value
    from gr_relational_analysis import Analysis

    fixture = run / "benchmark-fixtures-corrected/incidence-768"
    payload = fixture.with_suffix(".bin").read_bytes()
    metadata = json.loads(fixture.with_suffix(".json").read_text())
    engine = Analysis()
    with Context(modules=["gr.data.relational@1"]) as context:
        with context.import_value(ValueSpec._decode(metadata["spec"]), Value.bytes(payload),
                                  provenance=metadata["provenance"]) as value:
            def export_payload():
                exported = value.payload()
                try:
                    if exported.data != payload:
                        raise ValueError("Exported bytes differ")
                finally:
                    exported.close()

            def raw_import():
                with engine.import_parts(**metadata, payload=payload):
                    pass

            def bridge_import():
                with engine.import_value(value):
                    pass

            operations = {"descriptor_copy": value.describe, "payload_export_copy": export_payload,
                          "raw_import_close": raw_import, "bridge_import_close": bridge_import}
            for operation in operations.values():
                operation()
            samples = {name: [] for name in operations}
            order = []
            for repeat in range(5):
                names = list(operations)
                if repeat % 2:
                    names.reverse()
                order.append(names)
                for name in names:
                    start = time.perf_counter_ns()
                    operations[name]()
                    samples[name].append((time.perf_counter_ns() - start) / 1e6)
    report = {"format": "gr.python-bridge-profile/1", "python": sys.version,
              "scope": "Five alternating-order repeats after one warmup per operation; payload export includes byte equality and close. Full imports include validation/indexing and close. No allocation or sampled-CPU attribution.",
              "argv": sys.argv, "package_run": str(run), "native_libraries": artifacts["libraries"],
              "python_install": artifacts["python"], "source_bytes": len(payload),
              "source_sha256": hashlib.sha256(payload).hexdigest(), "order": order,
              "phases": {name: {"samples_ms": values, "median_ms": statistics.median(values)}
                         for name, values in samples.items()}}
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({name: phase["median_ms"] for name, phase in report["phases"].items()}))


if __name__ == "__main__":
    main()
