"""Installed wheel composition over artifacts from the installed C++ consumer."""

import ctypes
import gc
import json
import os
from pathlib import Path
import sys
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor

import gr_os
from gr_os.modules import discover, ModuleError

FIXTURE = Path(os.environ["GR_MODULE_FIXTURE"])
META = json.loads((FIXTURE / "incidence-meta.json").read_text(encoding="utf-8"))
PAYLOAD = (FIXTURE / "incidence.bin").read_bytes()


class Composition(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert gr_os.__version__ == "0.7.0"
        # Discovery is metadata enumeration and does not import advertised code.
        assert "gr_relational_analysis" not in sys.modules
        assert "gr_visualization" not in sys.modules
        entries = discover()
        assert len(entries) == 3
        assert "gr_relational_analysis" not in sys.modules
        assert "gr_visualization" not in sys.modules
        inventory = {"gr-os": "0.7.0", **{e.id: e.version for e in entries}}
        for entry in entries:
            document = entry.activate(available=inventory)
            assert document.id == entry.id
            try:
                entry.activate(available={})
            except ModuleError:
                pass
            else:
                raise AssertionError("Missing package dependency was accepted")
        import gr_relational_analysis as analysis
        import gr_visualization as visualization
        import gr_diagonal

        cls.analysis, cls.visualization, cls.diagonal = (
            analysis,
            visualization,
            gr_diagonal,
        )
        cls.engine = analysis.Analysis()
        cls.renderer = visualization.Visualization()

    def network(self):
        return self.engine.import_parts(**META, payload=PAYLOAD)

    def test_native_and_python_identity_and_composition(self):
        expected_snapshot = json.loads(
            (FIXTURE / "snapshot.json").read_text(encoding="utf-8")
        )
        with self.network() as network:
            self.assertEqual(
                network.describe()["identity"], expected_snapshot["identity"]
            )
            summary = network.summary()
            self.assertEqual(
                summary,
                json.loads((FIXTURE / "summary.json").read_text(encoding="utf-8")),
            )
            self.assertEqual(
                (summary["owners"], summary["targets"], summary["occurrences"]),
                (2, 3, 5),
            )
            snapshot = network.snapshot()
            self.assertEqual(snapshot.describe(), expected_snapshot)
            self.assertEqual(network.compare(network)["records"]["changed"], [])
        self.assertTrue(network.closed)
        scene = self.renderer.render(snapshot)
        self.assertEqual(scene.validation_basis, "structural_only")
        self.assertEqual(
            scene.describe(),
            json.loads((FIXTURE / "imported-scene.json").read_text(encoding="utf-8")),
        )
        self.assertEqual(
            scene.svg, (FIXTURE / "imported-scene.svg").read_text(encoding="utf-8")
        )
        model = json.loads((FIXTURE / "degree-model.json").read_text(encoding="utf-8"))
        weighted = self.diagonal.Diagonal().apply(
            [v["weight"] for v in model["mapping"]],
            [v["occurrence_degree"] for v in model["mapping"]],
        )
        self.assertEqual(sum(weighted.values), model["weighted_sum"])

    def test_main_sdk_explicit_copy(self):
        from gr_os.data import Context, ValueSpec
        from gr_os.execution import Value

        with Context(modules=["gr.data.relational@1"]) as context:
            with context.import_value(
                ValueSpec._decode(META["spec"]),
                Value.bytes(PAYLOAD),
                provenance=META["provenance"],
            ) as value:
                value.provenance.clear()  # Convenience Python copy is not authoritative.
                imported = self.engine.import_value(value)
        with imported:
            self.assertEqual(imported.summary()["occurrences"], 5)
            original = json.loads(
                (FIXTURE / "snapshot.json").read_text(encoding="utf-8")
            )
            self.assertEqual(imported.describe()["identity"], original["identity"])

    def test_rejected_main_buffer_export_closes_before_traceback_release(self):
        from unittest.mock import patch
        from gr_os.data import Context

        with Context() as context, context.column([1, 2, 3]) as value:
            exported = value.payload()
            self.assertEqual(exported.representation, "buffers")
            retained_error = None
            with patch.object(value, "payload", return_value=exported):
                try:
                    self.engine.import_value(value)
                except TypeError as error:
                    retained_error = error
                else:
                    self.fail("A numerical buffer was accepted as incidence bytes")
            self.assertEqual(
                str(retained_error),
                "Analysis requires a qualified incidence byte carrier",
            )
            self.assertIsNotNone(retained_error.__traceback__)
            self.assertTrue(exported.data.buffers)
            self.assertTrue(all(b.closed for b in exported.data.buffers.values()))
            self.assertEqual(value.tolist(), [1, 2, 3])

    def test_portable_import_tamper_and_mutability(self):
        with self.network() as network:
            snapshot = network.snapshot()
        decoded = self.engine.decode_snapshot(snapshot.encode())
        self.assertEqual(decoded.snapshot_id, snapshot.snapshot_id)
        self.assertEqual(decoded.validation_basis, "structural_only")
        description = decoded.describe()
        description["occurrences"][0]["role"] = "forged"
        with self.assertRaises(self.analysis.ModuleError) as caught:
            self.engine.decode_snapshot(json.dumps(description).encode())
        self.assertIn(
            caught.exception.code, {"analysis_integrity", "analysis_contract"}
        )
        # Fabricated Python labels are never trusted by the native renderer.
        forged = self.analysis.NetworkSnapshot(
            json.dumps(description).encode(), snapshot.snapshot_id, "source_derived"
        )
        with self.assertRaises(self.visualization.ModuleError):
            self.renderer.render(forged)
        self.assertNotEqual(decoded.describe(), description)

    def test_provenance_is_part_of_identity(self):
        changed = dict(META)
        changed["provenance"] = {
            "explicit_import": "alternate computational attribution"
        }
        with (
            self.network() as a,
            self.engine.import_parts(**changed, payload=PAYLOAD) as b,
        ):
            self.assertNotEqual(
                a.describe()["identity"]["source_value_id"],
                b.describe()["identity"]["source_value_id"],
            )

    def test_svg_options_and_escape(self):
        with self.network() as network:
            snapshot = network.snapshot()
        first = self.renderer.render(snapshot, title="<script>alert(1)</script>&")
        second = self.renderer.render(snapshot, title="<script>alert(1)</script>&")
        self.assertEqual(first.svg, second.svg)
        self.assertNotIn("<script>", first.svg)
        self.assertIn("&lt;script&gt;", first.svg)
        with self.assertRaises(self.visualization.ModuleError):
            self.renderer.render(snapshot, made_up_option=True)

    def test_close_and_concurrent_calls(self):
        network = self.network()
        with ThreadPoolExecutor(max_workers=4) as pool:
            summaries = list(pool.map(lambda _: network.summary(), range(16)))
        self.assertTrue(all(s["occurrences"] == 5 for s in summaries))
        gate = threading.Barrier(2)

        def racing_call():
            gate.wait()
            try:
                return network.summary()["occurrences"]
            except (ValueError, self.analysis.ModuleError) as error:
                if isinstance(error, self.analysis.ModuleError):
                    self.assertEqual(error.code, "invalid_handle")
                return "closed"

        with ThreadPoolExecutor(max_workers=1) as pool:
            future = pool.submit(racing_call)
            gate.wait()
            network.close()
            self.assertIn(future.result(), (5, "closed"))
        network.close()
        with self.assertRaises(ValueError):
            network.summary()

    def test_handle_limit_and_failure_cleanup(self):
        from unittest.mock import patch

        with patch.object(
            self.analysis, "Network", side_effect=MemoryError("wrapper failure")
        ):
            with self.assertRaises(MemoryError):
                self.network()
        for _ in range(66):
            with self.assertRaises(self.analysis.ModuleError):
                self.engine.import_parts(spec={}, provenance={}, payload=b"invalid")
        live = []
        try:
            for _ in range(64):
                live.append(self.network())
            with self.assertRaises(self.analysis.ModuleError) as caught:
                self.network()
            self.assertEqual(caught.exception.code, "resource_limit")
        finally:
            for network in live:
                network.close()
        ephemeral = self.network()
        del ephemeral
        gc.collect()
        with self.network() as restored:
            self.assertEqual(restored.summary()["occurrences"], 5)

    def test_c_transport_invalid_and_duplicate_metadata(self):
        native = self.engine._native
        response, size = ctypes.c_void_p(), ctypes.c_size_t()
        code = native._invoke(
            None, 1, None, 0, ctypes.byref(response), ctypes.byref(size)
        )
        try:
            self.assertEqual(code, 1)
        finally:
            native._free(response)
        duplicate = b'{"op":"manifest","op":"manifest"}'
        response, size = ctypes.c_void_p(), ctypes.c_size_t()
        code = native._invoke(
            duplicate,
            len(duplicate),
            None,
            0,
            ctypes.byref(response),
            ctypes.byref(size),
        )
        try:
            self.assertEqual(code, 1)
        finally:
            native._free(response)
        with self.assertRaises(self.analysis.ModuleError) as caught:
            native.call("summary", handle="network:missing")
        self.assertEqual(caught.exception.code, "invalid_handle")
        with self.assertRaises(self.analysis.ModuleError):
            native.call("manifest", unexpected=True)
        self.assertEqual(native.call("manifest"), self.analysis.manifest())
        self.assertEqual(
            self.renderer._native.call("manifest"), self.visualization.manifest()
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
