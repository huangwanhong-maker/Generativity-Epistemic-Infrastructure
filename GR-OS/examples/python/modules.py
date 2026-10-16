"""Compose installed independent packages over the installed C++ demo fixture.

The example writes only to the explicitly selected output directory. Its uniform
degree weighting is a declared illustration, not evidence of social importance.
"""

import argparse
import json
from pathlib import Path

from gr_diagonal import Diagonal
from gr_relational_analysis import Analysis
from gr_visualization import Visualization


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    metadata = json.loads(
        (args.fixture / "incidence-meta.json").read_text(encoding="utf-8")
    )
    payload = (args.fixture / "incidence.bin").read_bytes()
    analysis, visualization = Analysis(), Visualization()
    with analysis.import_parts(**metadata, payload=payload) as network:
        summary = network.summary()
        snapshot = network.snapshot()
    args.output.mkdir(parents=True, exist_ok=True)
    for layout in ("incidence_columns", "temporal_bands"):
        scene = visualization.render(
            snapshot, layout=layout, title="Synthetic qualified participation"
        )
        (args.output / (layout + ".svg")).write_text(scene.svg, encoding="utf-8")
        (args.output / (layout + ".json")).write_bytes(scene.encode())
    targets = [v for v in summary["degrees"] if v["kind"] == "participant_target"]
    weighted = Diagonal().apply(
        [2] * len(targets), [v["occurrence_degree"] for v in targets]
    )
    result = {
        "snapshot_id": snapshot.snapshot_id,
        "validation_basis": "structural_only",
        "mapping": targets,
        "weight": 2,
        "weighted_sum": sum(weighted.values),
        "assumption": "Uniform illustrative weight on recorded target occurrence degree",
        "model_provenance": weighted.report["provenance"],
    }
    (args.output / "model.json").write_text(
        json.dumps(result, indent=2), encoding="utf-8"
    )
    print(
        json.dumps(
            {
                "owners": summary["owners"],
                "occurrences": summary["occurrences"],
                "weighted_sum": result["weighted_sum"],
            }
        )
    )


if __name__ == "__main__":
    main()
