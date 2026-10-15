"""Record a reproducible unchanged Layer 4 graph under the selected native SDK.

Select GR_OS_LIBRARY and PYTHONPATH explicitly for each preserved/current SDK.
This does not reconstruct the old v0.5 capture whose input bytes were not recorded.
"""

import argparse
import json
from pathlib import Path

import gr_os as gr

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
args = parser.parse_args()
payload = b"GR-OS default-plan compatibility fixture v1"
with gr.Runtime(workers=1) as runtime:
    graph = gr.Graph(
        [gr.Node("identity", "gr.bytes.identity@1", [gr.Value.bytes(payload)])],
        [gr.OutputRef("identity")],
    )
    with runtime.plan(graph) as plan:
        result = {
            "sdk": gr.__version__,
            "fixture": {
                "node": "identity",
                "operation": "gr.bytes.identity@1",
                "payload_hex": payload.hex(),
                "output": {"node": "identity", "output": 0},
            },
            "plan": plan.description,
        }
args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
