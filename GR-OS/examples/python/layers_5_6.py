"""Run with PYTHONPATH=GR-OS/bindings/python/src and GR_OS_LIBRARY selected.

This example touches a fresh temporary SQLite database only. There is no server,
account provisioning, application, import/migration, or persistent demo runtime.
"""

import hashlib
from pathlib import Path
from tempfile import TemporaryDirectory

from gr_os import Storage, construct, execute, graph, validate


def main():
    material = b"A synthetic observation.\n"
    digest = "sha256:" + hashlib.sha256(material).hexdigest()
    observation = construct(
        {
            "format": "gr.record/1",
            "id": "observation:1",
            "type": "gr.assertion@1",
            "payload": {"text": "The instrument reported a change."},
            "attribution": {"agent": "example:observer", "method": "synthetic-example"},
            "qualifiers": {
                "epistemic_status": "reported; not independently established"
            },
            "artifacts": [
                {
                    "sha256": digest,
                    "byte_length": len(material),
                    "media_type": "text/plain",
                }
            ],
        }
    )
    with TemporaryDirectory(prefix="gr-os-example-") as temporary:
        with Storage(Path(temporary) / "records.sqlite") as storage:
            storage.create("inquiry")
            receipt = storage.commit(
                "inquiry",
                [observation],
                expected_revision="0",
                transaction_id="example:write:1",
                actor="example:observer",
                reason="Retain synthetic observation",
                blobs=[material],
            )
            snapshot = storage.snapshot("inquiry", receipt["revision"])
            assert validate(snapshot)["valid"]
            assert storage.blob("inquiry", digest) == material
            reader = storage.open_blob("inquiry", digest, chunk_bytes=8)
            print("Accepted revision:", receipt["revision"])
            print("Graph nodes:", len(graph(snapshot)["nodes"]))
        with reader:
            assert b"".join(reader.chunks()) == material
    result = execute("sha256", [material], backend="cpu.threaded", max_threads=2)
    assert result.outputs[0].decode("ascii") == digest.removeprefix("sha256:")
    print("Execution:", result.metadata)


if __name__ == "__main__":
    main()
