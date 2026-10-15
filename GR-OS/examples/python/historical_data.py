"""Small historical-data experiment through the same native C++ implementation."""

import struct
import gr_os as gr
from gr_os.experimental import analyze

with gr.Storage(backend="memory") as store:
    store.create("study")
    record = gr.construct(
        {
            "format": "gr.record/1",
            "id": "measurements",
            "type": "gr.entity@1",
            "payload": {"values": [1, 2, 3, 4]},
            "attribution": {"agent": "example", "method": "synthetic observations"},
        }
    )
    with store.begin_transaction(
        "study",
        expected_revision="",
        transaction_id="seed",
        actor="example",
        reason="Create synthetic fixture",
        track_history=True,
        admission="serialized",
    ) as tx:
        tx.stage([record])
        tx.commit()
    selected = store.history_snapshot("study")["provenance"]["semantic_revision"]
    with analyze(
        store,
        "study",
        "measurements",
        "/values",
        semantic_revision=selected,
        multiplier=3,
        addend=-1,
    ) as result:
        assert struct.unpack(
            "<q", result.outputs[1].data.buffers["values"].to_bytes()
        ) == (26,)
        print(result.provenance)
