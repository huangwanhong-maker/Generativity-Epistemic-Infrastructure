"""Native ownership, physical views, historical selection and optional Layer 3 use."""

import base64
import gc
import json
import os
import struct
import subprocess
import threading
import unittest
from unittest.mock import patch

import gr_os as gr
from gr_os import experimental

LAYOUT = {
    "element": {
        "format_id": "gr.int64@1",
        "item_size": 8,
        "alignment": 8,
        "byte_order": "little",
    },
    "shape": [4],
}


def record(values):
    return gr.construct(
        {
            "format": "gr.record/1",
            "id": "dataset",
            "type": "gr.entity@1",
            "payload": {"observations": values},
            "attribution": {"agent": "sdk-test", "method": "synthetic fixture"},
        }
    )


class BufferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = gr.Native()

    def test_copy_import_and_owned_readonly_export(self):
        source = bytearray(struct.pack("<4q", 1, 2, 3, 4))
        value = gr.Buffer(source, layout=LAYOUT, native=self.native)
        with patch("gr_os._native.sys.version_info", (3, 10)):
            with self.assertRaises(RuntimeError):
                value.memoryview()
            self.assertEqual(value.to_bytes(), bytes(source))
        exported = value.memoryview()
        source[:] = bytes(len(source))
        self.assertEqual(exported.tolist(), [1, 2, 3, 4])
        self.assertTrue(exported.readonly)
        with self.assertRaises(TypeError):
            exported[0] = 99
        self.assertTrue(memoryview(exported.obj).readonly)
        with self.assertRaises(TypeError):
            exported.obj.__class__()
        with self.assertRaises(TypeError):
            memoryview(exported.obj)[0] = 99
        child = exported[1:]
        value.close()
        exported.release()
        gc.collect()
        self.assertEqual(child.tolist(), [2, 3, 4])
        child.release()

    def test_strides_copy_and_lifetime(self):
        with gr.Buffer(
            struct.pack("<4q", 2, 4, 6, 8), layout=LAYOUT, native=self.native
        ) as original:
            reverse = original.slice(3, 4, -1)
        with reverse:
            view = reverse.memoryview()
            self.assertEqual(view.strides, (-8,))
            self.assertEqual(view.tolist(), [8, 6, 4, 2])
            self.assertEqual(struct.unpack("<4q", reverse.to_bytes()), (8, 6, 4, 2))
            with self.assertRaises(gr.NativeError):
                reverse.readinto(bytearray(32))
            with reverse.copy_contiguous() as copied:
                self.assertTrue(copied.contiguous)
            view.release()

    def test_scalar_and_opaque_parameterized_export(self):
        for element, expected_format in (
            (
                {
                    "format_id": "gr.int64@1",
                    "item_size": 8,
                    "alignment": 8,
                    "byte_order": "little",
                },
                "q",
            ),
            (
                {
                    "format_id": "gr.int64@1",
                    "item_size": 8,
                    "parameters": {"meaning": "opaque"},
                },
                "8s",
            ),
        ):
            with gr.Buffer(
                struct.pack("<q", 7),
                layout={"element": element, "shape": []},
                native=self.native,
            ) as value:
                view = value.memoryview()
                self.assertEqual(view.ndim, 0)
                self.assertEqual(view.format, expected_format)
                self.assertEqual(view.tobytes(), struct.pack("<q", 7))
                view.release()

    def test_invalid_layout_and_offsets(self):
        for layout in (
            {"shape": [100]},
            {"shape": [2**63]},
            {"surprise": True},
            {
                "element": {"item_size": 8, "alignment": 8},
                "shape": [1],
                "byte_offset": 1,
            },
        ):
            with self.assertRaises(gr.NativeError):
                gr.Buffer(b"12345678", layout=layout, native=self.native)
        with gr.Buffer(b"abc", native=self.native) as value:
            for offset in (-1, 2**64, True):
                with self.assertRaises(ValueError):
                    value.readinto(bytearray(1), offset)

    def test_close_race_keeps_inflight_owner(self):
        for _ in range(20):
            value = gr.Buffer(b"immutable", native=self.native)
            barrier = threading.Barrier(2)
            failures = []

            def read():
                barrier.wait()
                try:
                    view = value.memoryview()
                    if view.tobytes() != b"immutable":
                        failures.append("corrupt")
                    view.release()
                except (gr.NativeError, BufferError):
                    pass  # close won admission

            thread = threading.Thread(target=read)
            thread.start()
            barrier.wait()
            value.close()
            thread.join()
            self.assertEqual(failures, [])

    def test_plan_owns_buffer_after_python_handle_close(self):
        with gr.Runtime(
            native=self.native, modules=["gr.experimental.data@1"]
        ) as runtime:
            source = gr.Buffer(
                struct.pack("<4q", 1, 2, 3, 4), layout=LAYOUT, native=self.native
            )
            argument = gr.Value.buffers(
                "gr.experimental.int64-column@1", gr.BufferSet({"values": source})
            )
            graph = gr.Graph(
                [gr.Node("sum", "gr.experimental.int64-sum@1", [argument])],
                [gr.OutputRef("sum")],
            )
            with runtime.plan(graph) as plan:
                source.close()
                with runtime.run(plan).raise_for_status() as result:
                    output = result.outputs[0].data.buffers["values"]
                    self.assertEqual(struct.unpack("<q", output.to_bytes()), (10,))
                    exported = output.memoryview()
            self.assertEqual(exported.tolist(), [10])
            exported.release()

    def test_exact_overflow_is_failure_and_extension_is_optional(self):
        with gr.Buffer(
            struct.pack("<q", 2**63 - 1),
            layout={**LAYOUT, "shape": [1]},
            native=self.native,
        ) as source:
            column = gr.Value.buffers(
                "gr.experimental.int64-column@1", gr.BufferSet({"values": source})
            )
            graph = gr.Graph(
                [
                    gr.Node(
                        "x",
                        "gr.experimental.int64-transform@1",
                        [
                            column,
                            gr.Value.json("gr.json@1", {"multiplier": 1, "addend": 1}),
                        ],
                    )
                ],
                [gr.OutputRef("x")],
            )
            with gr.Runtime(native=self.native) as runtime:
                with self.assertRaises(gr.NativeError):
                    runtime.plan(graph)
            with gr.Runtime(
                native=self.native, modules=["gr.experimental.data@1"]
            ) as runtime:
                with runtime.run(graph) as result:
                    self.assertEqual(result.status, "failed")
                    self.assertEqual(result.outputs, [])

    def test_selected_history_and_serial_threaded_agree(self):
        with gr.Storage(backend="memory", native=self.native) as store:
            store.create("research")
            for index, values in enumerate(([1, 2, 3, 4], [100, 200])):
                with store.begin_transaction(
                    "research",
                    expected_revision="",
                    transaction_id=f"tx{index}",
                    actor="test",
                    reason="synthetic change",
                    track_history=True,
                    admission="serialized",
                ) as tx:
                    tx.stage([record(values)])
                    tx.commit()
                if index == 0:
                    selected = store.history_snapshot("research")
                    old = selected["provenance"]["semantic_revision"]
            self.assertEqual(len(store.history_export_v1("research")["nodes"]), 2)
            for invalid in (1.5, True, 2**64 - 1):
                with self.assertRaises(gr.NativeError):
                    store._call(
                        "experimental.data.analyze",
                        workspace="research",
                        record_id="dataset",
                        payload_pointer="/observations",
                        multiplier=invalid,
                    )
            answers = []
            for serial in (True, False):
                with experimental.analyze(
                    store,
                    "research",
                    "dataset",
                    "/observations",
                    semantic_revision=old,
                    multiplier=3,
                    addend=-1,
                    serial=serial,
                ) as result:
                    answers.append(
                        [v.data.buffers["values"].to_bytes() for v in result.outputs]
                    )
                    self.assertEqual(result.provenance["semantic_revision"], old)
                    self.assertEqual(
                        result.provenance["record_version"],
                        selected["records"][0]["version"],
                    )
                    self.assertIn("recipe", result.provenance)
            self.assertEqual(answers[0], answers[1])
            self.assertEqual(struct.unpack("<4q", answers[0][0]), (2, 5, 8, 11))
            self.assertEqual(struct.unpack("<q", answers[0][1]), (26,))

    def test_cli_binary_fallback_session(self):
        cli = os.environ.get("GR_OS_CLI")
        if not cli:
            self.skipTest("CLI disabled")
        process = subprocess.Popen(
            [cli, "session"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        def call(op, **params):
            process.stdin.write(json.dumps({"abi": 1, "op": op, **params}) + "\n")
            process.stdin.flush()
            result = json.loads(process.stdout.readline())
            self.assertTrue(result["ok"], result)
            return result["result"]

        try:
            buffer = call(
                "buffer.import_base64",
                data_base64=base64.b64encode(b"cli-buffer").decode(),
            )
            exported = call("buffer.export_base64", handle=buffer["handle"])
            self.assertEqual(base64.b64decode(exported["data_base64"]), b"cli-buffer")
            call("buffer.close", handle=buffer["handle"])
            store = call("store.open", backend="memory")
            call("store.create", handle=store["handle"], workspace="cli-study")
            call(
                "store.commit",
                handle=store["handle"],
                workspace="cli-study",
                expected_revision="0",
                transaction_id="seed",
                actor="cli",
                reason="synthetic fixture",
                track_history=True,
                records=[record([1, 2, 3, 4])],
            )
            analyzed = call(
                "experimental.data.analyze",
                handle=store["handle"],
                workspace="cli-study",
                record_id="dataset",
                payload_pointer="/observations",
                multiplier=3,
                addend=-1,
            )
            self.assertEqual(analyzed["status"], "succeeded")
            self.assertIn("semantic_revision", analyzed["provenance"])
            output_handle = analyzed["outputs"][1]["data"]["buffers"][0]["handle"]
            exported = call("buffer.export_base64", handle=output_handle)
            self.assertEqual(
                struct.unpack("<q", base64.b64decode(exported["data_base64"])), (26,)
            )
        finally:
            process.stdin.close()
            process.wait(timeout=10)
            self.assertEqual(process.returncode, 0, process.stderr.read())
            process.stdout.close()
            process.stderr.close()


if __name__ == "__main__":
    unittest.main()
