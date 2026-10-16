"""Deterministic native incidence scenes and SVG over copied typed snapshots."""

from __future__ import annotations
from dataclasses import dataclass
from importlib.resources import files
import json
from ._native import Native, ModuleError

__version__ = "0.1.0"


def manifest() -> dict:
    return json.loads(
        files(__package__).joinpath("manifest.json").read_text(encoding="utf-8")
    )


@dataclass(frozen=True)
class RenderedScene:
    _document: bytes
    svg: str
    snapshot_id: str
    validation_basis: str

    def describe(self) -> dict:
        return json.loads(self._document)

    def encode(self) -> bytes:
        return self._document


class Visualization:
    def __init__(self, library=None):
        self._native = Native(library)

    def render(self, snapshot, **options) -> RenderedScene:
        """Validate actual snapshot bytes, then project and render natively.

        All source vertices remain in the constructed layout. Role/temporal
        filters select occurrences only; geometry is not physical evidence.
        Output strings/bytes are caller-owned. No files are written by this API.
        """
        from gr_relational_analysis import NetworkSnapshot

        if not isinstance(snapshot, NetworkSnapshot):
            raise TypeError("render requires a typed NetworkSnapshot")
        result = self._native.call("render", payload=snapshot.encode(), options=options)
        encoded = json.dumps(
            result["scene"], ensure_ascii=False, allow_nan=False, separators=(",", ":")
        ).encode("utf-8")
        return RenderedScene(
            encoded, result["svg"], result["snapshot_id"], result["validation_basis"]
        )


__all__ = ["Visualization", "RenderedScene", "ModuleError", "manifest"]
