"""Optional Layer 3 historical-data proof of concept; all computation is native."""

from __future__ import annotations
from dataclasses import dataclass
from .execution import Value
from .storage import Storage
from ._native import NativeError


@dataclass
class AnalysisResult:
    outputs: list[Value]
    provenance: dict
    diagnostics: dict

    def close(self):
        for value in self.outputs:
            value.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


def analyze(
    store: Storage,
    workspace: str,
    record_id: str,
    payload_pointer: str,
    *,
    semantic_revision: str = "",
    physical_revision: str = "",
    multiplier: int = 1,
    addend: int = 0,
    workers: int = 2,
    serial: bool = False,
) -> AnalysisResult:
    for name, value in {"multiplier": multiplier, "addend": addend}.items():
        if type(value) is not int or abs(value) > 9007199254740991:
            raise ValueError(f"{name} must be a canonical metadata integer")
    result = store._call(
        "experimental.data.analyze",
        workspace=workspace,
        record_id=record_id,
        payload_pointer=payload_pointer,
        semantic_revision=semantic_revision,
        physical_revision=physical_revision,
        multiplier=multiplier,
        addend=addend,
        workers=workers,
        serial=serial,
    )
    if result["status"] != "succeeded":
        error = result["error"] or {}
        raise NativeError(
            error.get("code", result["status"]),
            error.get("message", "Analysis failed"),
            result,
        )
    return AnalysisResult(
        [Value._decode(v, store.native) for v in result["outputs"]],
        result["provenance"],
        result["diagnostics"],
    )


def select(
    store: Storage,
    workspace: str,
    record_id: str,
    payload_pointer: str,
    *,
    semantic_revision: str = "",
    physical_revision: str = "",
) -> tuple[Value, dict]:
    result = store._call(
        "experimental.data.select",
        workspace=workspace,
        record_id=record_id,
        payload_pointer=payload_pointer,
        semantic_revision=semantic_revision,
        physical_revision=physical_revision,
    )
    return Value._decode(result["column"], store.native), result["provenance"]
