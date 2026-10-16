"""Installed-package transport costs over native benchmark's exact fixtures.

Run only in a reserved measurement window. Requires the three explicitly selected
GR native libraries and installed gr_os/analysis/visualization wheels. Outputs are
validated outside timing; Python result copying is intentionally included.
"""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import time

import gr_os
from gr_os.data import Context, ValueSpec
from gr_os.execution import Value
from gr_relational_analysis import Analysis, manifest
from gr_visualization import Visualization


def measure(function, repeats):
    function()
    samples = []
    for _ in range(repeats):
        start = time.perf_counter_ns()
        function()
        samples.append((time.perf_counter_ns() - start) / 1e6)
    return {
        "samples_ms": samples,
        "median_ms": statistics.median(samples),
        "min_ms": min(samples),
        "max_ms": max(samples),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixtures", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--repeats", type=int, default=7, choices=range(1, 11))
    args = parser.parse_args()
    engine, visual = Analysis(), Visualization()
    report = {
        "format": "gr.module-python-benchmark/1",
        "python": platform.python_version(),
        "gr_os": gr_os.__version__,
        "analysis_package": manifest(),
        "repeats": args.repeats,
        "warmups": 1,
        "scope": "Installed blocking Python facade; copies and response decoding included. No RSS/zero-copy claim.",
        "timing_scope": "Warm engines after one phase warmup; timed result destruction included. Not process-start or cold-validation latency.",
        "cases": [],
    }
    with Context(modules=["gr.data.relational@1"]) as context:
        for count in (64, 256, 768):
            stem = args.fixtures / f"incidence-{count}"
            source = stem.with_suffix(".bin").read_bytes()
            metadata = json.loads(stem.with_suffix(".json").read_text(encoding="utf-8"))
            next_stem = args.fixtures / f"incidence-next-{count}"
            next_bytes = next_stem.with_suffix(".bin").read_bytes()
            next_meta = json.loads(
                next_stem.with_suffix(".json").read_text(encoding="utf-8")
            )
            with (
                engine.import_parts(**metadata, payload=source) as network,
                engine.import_parts(**next_meta, payload=next_bytes) as changed,
                context.import_value(
                    ValueSpec._decode(metadata["spec"]),
                    Value.bytes(source),
                    provenance=metadata["provenance"],
                ) as main_value,
            ):
                summary = network.summary()
                snapshot = network.snapshot()
                rendered = visual.render(snapshot)
                comparison = network.compare(changed)
                assert summary["owners"] == count
                assert summary["targets"] == 16 and summary["occurrences"] == count * 3
                assert len(comparison["records"]["changed"]) == 1
                assert rendered.validation_basis == "structural_only"
                with engine.import_value(main_value) as copied:
                    assert (
                        copied.describe()["identity"] == network.describe()["identity"]
                    )
                assert (
                    engine.decode_snapshot(snapshot.encode()).snapshot_id
                    == snapshot.snapshot_id
                )

                def import_raw():
                    with engine.import_parts(**metadata, payload=source):
                        pass

                def import_main():
                    with engine.import_value(main_value):
                        pass

                def end_to_end():
                    with engine.import_parts(**metadata, payload=source) as value:
                        value.summary()
                        visual.render(value.snapshot())

                functions = {
                    "raw_import_and_close": import_raw,
                    "gr_os_copy_bridge_and_close": import_main,
                    "persistent_summary": network.summary,
                    "persistent_compare": lambda: network.compare(changed),
                    "compact_export": network.snapshot,
                    "compact_decode": lambda: engine.decode_snapshot(snapshot.encode()),
                    "compact_to_scene_svg": lambda: visual.render(snapshot),
                    "raw_import_summary_scene_svg_close": end_to_end,
                }
                row = {
                    "owners": count,
                    "occurrences": summary["occurrences"],
                    "source_bytes": len(source),
                    "snapshot_bytes": len(snapshot.encode()),
                    "scene_bytes": len(rendered.encode()),
                    "svg_utf8_bytes": len(rendered.svg.encode()),
                    "source_sha256": hashlib.sha256(source).hexdigest(),
                    "snapshot_id": snapshot.snapshot_id,
                    "scene_sha256": hashlib.sha256(rendered.encode()).hexdigest(),
                    "phases": {
                        name: measure(fn, args.repeats)
                        for name, fn in functions.items()
                    },
                }
                report["cases"].append(row)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
