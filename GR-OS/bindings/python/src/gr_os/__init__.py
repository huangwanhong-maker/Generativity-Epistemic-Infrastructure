"""GR-OS: one native semantic implementation, Python transport and SDK helpers."""

from __future__ import annotations
from typing import Any, Iterable, Mapping
from ._native import Native, NativeError, _native
from .storage import Storage, BlobReader
from .buffers import Buffer, BufferSet
from .execution import (
    ExecutionResult,
    TypedExecutionResult,
    Value,
    devices,
    execution_capabilities,
    contracts,
    execute,
    execute_typed,
)

from .runtime import (
    Runtime,
    Plan,
    Job,
    JobResult,
    Graph,
    Node,
    OutputRef,
    Publication,
    ArtifactBinding,
    Transaction,
)

__version__ = "0.7.0"

__all__ = [
    "Native",
    "NativeError",
    "Storage",
    "BlobReader",
    "Buffer",
    "BufferSet",
    "ExecutionResult",
    "TypedExecutionResult",
    "Value",
    "devices",
    "execution_capabilities",
    "contracts",
    "execute",
    "execute_typed",
    "Runtime",
    "Plan",
    "Job",
    "JobResult",
    "Graph",
    "Node",
    "OutputRef",
    "Publication",
    "ArtifactBinding",
    "Transaction",
    "construct",
    "verify",
    "validate",
    "apply",
    "graph",
    "project",
    "match",
    "parse_metadata",
    "history_validate",
    "history_append",
    "history_reconstruct",
    "history_diff",
    "history_merge",
]


def construct(document: Mapping[str, Any], *, native: Native | None = None) -> dict:
    return _native(native).call("core.construct", document=document)


def verify(record: Mapping[str, Any], *, native: Native | None = None) -> dict:
    return _native(native).call("core.verify", record=record)


def validate(
    snapshot: Mapping[str, Any],
    *,
    for_mutation: bool = False,
    native: Native | None = None,
) -> dict:
    return _native(native).call(
        "core.validate", snapshot=snapshot, for_mutation=for_mutation
    )


def apply(
    snapshot: Mapping[str, Any],
    updates: Iterable[Mapping[str, Any]],
    *,
    native: Native | None = None,
) -> dict:
    return _native(native).call("core.apply", snapshot=snapshot, updates=list(updates))


def graph(snapshot: Mapping[str, Any], *, native: Native | None = None) -> dict:
    return _native(native).call("core.graph", snapshot=snapshot)


def project(
    snapshot: Mapping[str, Any],
    query: Mapping[str, Any] | None = None,
    *,
    native: Native | None = None,
) -> dict:
    """Project a declared event order/cut; this does not infer physical time."""
    return _native(native).call(
        "core.time", snapshot=snapshot, query={} if query is None else query
    )


def match(
    snapshot: Mapping[str, Any],
    filter: Mapping[str, Any] | None = None,
    *,
    native: Native | None = None,
) -> dict:
    return _native(native).call(
        "core.match", snapshot=snapshot, filter={} if filter is None else filter
    )


def parse_metadata(utf8: str, *, native: Native | None = None) -> Any:
    return _native(native).call("metadata.parse", utf8=utf8)


def history_validate(history: dict, *, native: Native | None = None) -> dict:
    return _native(native).call("history.validate", history=history)


def history_append(
    history: dict,
    snapshot: dict,
    parents: list[str],
    attribution: dict,
    *,
    native: Native | None = None,
) -> dict:
    return _native(native).call(
        "history.append",
        history=history,
        snapshot=snapshot,
        parents=parents,
        attribution=attribution,
    )


def history_reconstruct(
    history: dict, revision: str, *, native: Native | None = None
) -> dict:
    return _native(native).call(
        "history.reconstruct", history=history, revision=revision
    )


def history_diff(
    history: dict, left: str, right: str, *, native: Native | None = None
) -> dict:
    return _native(native).call("history.diff", history=history, left=left, right=right)


def history_merge(
    history: dict,
    base: str,
    left: str,
    right: str,
    resolutions: dict,
    attribution: dict,
    *,
    native: Native | None = None,
) -> dict:
    return _native(native).call(
        "history.merge",
        history=history,
        base=base,
        left=left,
        right=right,
        resolutions=resolutions,
        attribution=attribution,
    )
