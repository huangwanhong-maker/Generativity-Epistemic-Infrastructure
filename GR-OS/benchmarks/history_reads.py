"""Historical selection through each version's available public Python APIs.

Same record updates and user attribution; transport paths differ. Automatically
recorded physical-base provenance differs between representations, so newly
created semantic node IDs are checked within each version, not equated across it.
v0.3 obtains the complete bundle and calls its public reconstruction operation.
v0.4 calls the native selected-history helper. This is end-to-end selection cost.
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

import gr_os as gr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    if not 3 <= args.repeats <= 20:
        parser.error("repeats must be 3..20")
    compact = hasattr(gr.Storage, "history_snapshot")
    cases = []
    with tempfile.TemporaryDirectory(prefix="gr-history-reads-") as temporary:
        for backend in ("memory", "sqlite"):
            kwargs = {"backend": "memory"} if backend == "memory" else {"path": Path(temporary) / "history.sqlite"}
            with gr.Storage(**kwargs) as store:
                store.create("history")
                def item(index, generation):
                    return gr.construct({"format": "gr.record/1", "id": f"record:{index}",
                        "type": "gr.assertion@1", "payload": {"generation": generation, "padding": "x" * 256},
                        "attribution": {"agent": "benchmark", "method": "synthetic"}})
                physical, semantic = [], []
                for change in range(33):
                    records = [item(index, 0) for index in range(32)] if change == 0 else [item(change - 1, change)]
                    with gr.Transaction(store, "history", expected_revision="", transaction_id=f"change:{change}",
                                        actor="benchmark", reason="Historical selection fixture", track_history=True) as tx:
                        tx.stage(records)
                        accepted = tx.commit()
                    physical.append(accepted["publication"]["revision"])
                    semantic.append(accepted["history"]["head"])

                def select(physical_revision, semantic_revision=""):
                    if compact:
                        return store.history_snapshot("history", semantic_revision, physical_revision=physical_revision)
                    snapshot = store.snapshot("history", physical_revision)
                    info = snapshot["runtime_history"]
                    bundle = json.loads(store.blob("history", info["bundle_digest"]))
                    return gr.history_reconstruct(bundle, semantic_revision or info["head"])

                # Node positions 17/24/32/33 exercise default compact depths 0/7/15/0.
                for index in (16, 23, 31, 32):
                    for historical_lookup in (False, True):
                        pinned = physical[-1] if historical_lookup else physical[index]
                        selected = semantic[index] if historical_lookup else ""
                        result = select(pinned, selected)
                        samples = []
                        for _ in range(args.repeats):
                            start = time.perf_counter_ns()
                            result = select(pinned, selected)
                            samples.append((time.perf_counter_ns() - start) / 1e6)
                        assert result["revision"] == semantic[index]
                        assert len(result["records"]) == 32
                        assert sum(r["document"]["payload"]["generation"] for r in result["records"]) == index * (index + 1) // 2
                        cases.append({"backend": backend, "selected_node": index + 1,
                            "pinned_node": 33 if historical_lookup else index + 1,
                            "semantic_revision": semantic[index], "catalogue_lookup": historical_lookup,
                            "samples_ms": samples, "median_ms": statistics.median(samples),
                            "diagnostics": result.get("history_diagnostics"), "verified": True})
    result = {"suite": "gr.history.selection/1", "platform": platform.platform(), "python": platform.python_version(),
        "path": "native compact selection" if compact else "public snapshot + bundle transfer + v1 reconstruction",
        "native_sha256": hashlib.sha256(Path(os.environ["GR_OS_LIBRARY"]).read_bytes()).hexdigest(),
        "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "method": "32 records with 256-byte padding; seed plus 32 single updates; one query warmup; checks outside timing",
        "limitations": "Includes each available SDK transport path. Not pure codec timing. Tiny local histories, no branch/merge throughput claim.",
        "cases": cases}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
