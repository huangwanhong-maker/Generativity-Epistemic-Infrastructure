"""Metadata discovery and activation, including a real Python-only distribution fixture."""

import importlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from gr_os.modules import (ModuleError, discover, read_manifest, validate_manifest,
                           MAX_MANIFEST_BYTES)


def declaration():
    return {"format": "gr.module-package/1", "id": "org.example.python-tool",
            "version": "0.1.0", "categories": ["unanticipated-formalism"],
            "requires": [{"id": "gr-os", "version": "0.7.0"}],
            "exports": {"functions": ["example.increment@1"]},
            "effects": ["pure"], "trust": "Trusted in-process Python; no sandbox.",
            "compatibility": "Python 3.10+ source; SDK 0.7.0 metadata profile."}


class Modules(unittest.TestCase):
    def test_manifest_detaches_nested_data(self):
        source = declaration()
        manifest = validate_manifest(source)
        source["exports"]["functions"].clear()
        description = manifest.describe()
        self.assertEqual(description["exports"]["functions"], ["example.increment@1"])
        description["requires"].clear()
        self.assertEqual(len(manifest.describe()["requires"]), 1)

    def test_exact_dependency_check(self):
        manifest = validate_manifest(declaration())
        manifest.check_dependencies({"gr-os": "0.7.0"})
        for available in ({}, {"gr-os": "0.6.0"}, {"gr-os": "0.7.1"}):
            with self.assertRaises(ModuleError):
                manifest.check_dependencies(available)

    def test_no_effect_authority_or_closed_category_taxonomy(self):
        source = declaration()
        source["effects"] = ["external"]
        source["optional_future_metadata"] = {"x": 1}
        manifest = validate_manifest(source)
        self.assertEqual(manifest.describe()["effects"], ["external"])
        self.assertEqual(manifest.describe()["categories"], ["unanticipated-formalism"])
        self.assertEqual(manifest.describe()["optional_future_metadata"], {"x": 1})

    def test_unsupported_or_ambiguous_declarations(self):
        bad = [
            {"format": "gr.module-package/2"}, {"id": "../escape"},
            {"version": "0.01.0"}, {"categories": []}, {"effects": ["magic"]},
            {"effects": ["pure", "pure"]}, {"required_extensions": ["unknown"]},
            {"requires": [{"id": "gr-os", "version": ">=0.7"}]},
            {"requires": [{"id": "gr-os", "version": "0.7.0", "optional": True}]},
            {"requires": [{"id": "gr-os", "version": "0.7.0"}] * 2},
            {"requires": [{"id": "org.example.python-tool", "version": "0.1.0"}]},
            {"exports": {"functions": ["same", "same"]}}, {"exports": []},
            {"trust": ""}, {"optional": float("nan")}, {"optional": "x" * 65536},
        ]
        for change in bad:
            with self.subTest(change=list(change)):
                value = declaration()
                value.update(change)
                with self.assertRaises(ModuleError):
                    validate_manifest(value)

    def test_manifest_reader_limits_duplicates_utf8(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "manifest.json"
            path.write_text(json.dumps(declaration()), encoding="utf-8")
            self.assertEqual(read_manifest(path).id, "org.example.python-tool")
            for value in (b'{"id":"a","id":"b"}', b'\xff', b'[]', b'NaN',
                          b' ' * (MAX_MANIFEST_BYTES + 1)):
                path.write_bytes(value)
                with self.assertRaises(ModuleError):
                    read_manifest(path)

    def test_real_python_only_distribution_discovery_and_activation(self):
        # Real dist-info fixtures establish metadata discovery without importing
        # advertised code. Nothing here needs a native DLL or core source edit.
        module_name = "gr_test_python_only_module"
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            info = root / "gr_python_only_fixture-0.1.0.dist-info"
            info.mkdir()
            (info / "METADATA").write_text(
                "Metadata-Version: 2.1\nName: gr-python-only-fixture\nVersion: 0.1.0\n",
                encoding="utf-8")
            (info / "entry_points.txt").write_text(
                "[gr_os.modules]\norg.example.python-tool = " + module_name + ":manifest\n",
                encoding="utf-8")
            (root / (module_name + ".py")).write_text(
                "def manifest():\n    return " + repr(declaration()) +
                "\ndef increment(value):\n    return value + 1\n", encoding="utf-8")
            sys.path.insert(0, temp)
            importlib.invalidate_caches()
            try:
                entries = [entry for entry in discover() if entry.id == "org.example.python-tool"]
                self.assertEqual(len(entries), 1)
                self.assertNotIn(module_name, sys.modules)
                manifest = entries[0].activate(available={"gr-os": "0.7.0"})
                self.assertIn(module_name, sys.modules)
                self.assertEqual(manifest.version, "0.1.0")
                self.assertEqual(importlib.import_module(module_name).increment(3), 4)
                with self.assertRaisesRegex(ModuleError, "requires gr-os"):
                    entries[0].activate(available={"gr-os": "0.6.0"})
                # Metadata is checked again on activation, not accepted as a
                # substitute for the factory's actual declaration.
                module = sys.modules[module_name]
                with patch.object(module, "manifest", return_value={**declaration(), "version": "0.2.0"}):
                    with self.assertRaisesRegex(ModuleError, "does not match"):
                        entries[0].activate()
                from importlib import metadata
                point = next(point for point in metadata.entry_points(group="gr_os.modules")
                             if point.name == "org.example.python-tool")
                with patch("gr_os.modules.metadata.entry_points", return_value=[point, point]):
                    with self.assertRaisesRegex(ModuleError, "duplicate installed"):
                        discover()
                (info / "entry_points.txt").write_text(
                    "[gr_os.modules]\norg.example.python-tool = not valid !\n", encoding="utf-8")
                importlib.invalidate_caches()
                with self.assertRaisesRegex(ModuleError, "malformed entry point reference"):
                    discover()
            finally:
                sys.path.remove(temp)
                sys.modules.pop(module_name, None)
                importlib.invalidate_caches()


if __name__ == "__main__":
    unittest.main()
