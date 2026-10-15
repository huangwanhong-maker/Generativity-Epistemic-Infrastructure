"""Run a dependency graph and atomically publish computed bytes with history."""

import tempfile
from pathlib import Path
from gr_os import (
    ArtifactBinding,
    Graph,
    Native,
    Node,
    OutputRef,
    Publication,
    Runtime,
    Storage,
    Value,
)


def main():
    native = Native()
    graph = Graph(
        [
            Node("left", "gr.bytes.identity@1", [Value.bytes(b"relational ")]),
            Node("right", "gr.bytes.identity@1", [Value.bytes(b"being")]),
            Node(
                "joined", "gr.bytes.concat@1", [OutputRef("left"), OutputRef("right")]
            ),
            Node("digest", "gr.bytes.sha256@1", [OutputRef("joined")]),
        ],
        [OutputRef("joined"), OutputRef("digest")],
    )
    with tempfile.TemporaryDirectory(prefix="gr-runtime-example-") as directory:
        path = Path(directory) / "example.sqlite"
        with Storage(path, native=native) as store:
            store.create("example")
            with Runtime(
                native=native, workers=2, policy={"writable_workspaces": ["example"]}
            ) as runtime:
                publication = Publication(
                    store,
                    {
                        "workspace": "example",
                        "expected_revision": "0",
                        "transaction_id": "example:1",
                        "actor": "example",
                        "reason": "publish graph output",
                        "track_history": True,
                    },
                    [
                        ArtifactBinding(
                            OutputRef("joined"),
                            {
                                "format": "gr.record/1",
                                "id": "example:artifact",
                                "type": "gr.entity@1",
                                "payload": {"label": "computed greeting"},
                                "attribution": {
                                    "agent": "example",
                                    "method": "typed runtime graph",
                                },
                            },
                            "text/plain",
                        )
                    ],
                )
                with (
                    runtime.plan(graph) as plan,
                    runtime.submit(plan, publication=publication) as job,
                ):
                    result = job.result().raise_for_status()
                    assert result.outputs[0].data == b"relational being"
                    print(result.outputs[1].data.decode("ascii"))
                    print(result.publication)
            revision = store.head("example")
        with Storage(path, native=native) as reopened:
            assert reopened.head("example") == revision
            record = reopened.snapshot("example")["records"][0]
            digest = record["document"]["artifacts"][0]["sha256"]
            assert reopened.blob("example", digest) == b"relational being"
            assert reopened.receipt("example", "example:1") is not None


if __name__ == "__main__":
    main()
