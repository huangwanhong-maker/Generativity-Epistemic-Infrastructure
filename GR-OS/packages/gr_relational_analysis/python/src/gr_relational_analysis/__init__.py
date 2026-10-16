"""Qualified native analysis with retained source custody and copied snapshots.

Importing this package and reading manifest() do not load native code. Analysis
requires an explicit library selection. Snapshot interchange validates structure;
it does not prove historical derivation omitted from that projection.
"""

from __future__ import annotations
from dataclasses import dataclass
from importlib.resources import files
import json
import threading
import weakref
from ._native import Native, ModuleError

__version__ = "0.1.0"


def manifest() -> dict:
    return json.loads(
        files(__package__).joinpath("manifest.json").read_text(encoding="utf-8")
    )


@dataclass(frozen=True)
class NetworkSnapshot:
    """Immutable copied interchange, not an owner of omitted source records.

    Use Analysis.decode_snapshot for untrusted bytes. Visualization always
    validates the actual bytes again, irrespective of this object's labels.
    """

    _encoded: bytes
    snapshot_id: str
    validation_basis: str

    def __post_init__(self):
        if not isinstance(self._encoded, bytes):
            raise TypeError("Snapshot storage must be immutable bytes")
        if len(self._encoded) > 64 * 1024 * 1024:
            raise ValueError("Snapshot exceeds 64 MiB")

    def encode(self) -> bytes:
        return self._encoded

    def describe(self) -> dict:
        """Return a detached mutable copy of the compact projection."""
        return json.loads(self._encoded)


class Analysis:
    """Package-specific native engine. Calls are blocking and release the GIL.

    Each imported Network retains validated native source custody. This facade
    has no cross-DLL handle interoperability or Python cancellation promise.
    """

    def __init__(self, library=None):
        self._native = Native(library)

    def import_parts(self, *, spec: dict, provenance: dict, payload: bytes) -> Network:
        result = self._native.call(
            "import", spec=spec, provenance=provenance, payload=payload
        )
        try:
            return Network(self._native, result)
        except BaseException:
            self._native.call("close", handle=result["handle"])
            raise

    def import_value(self, value) -> Network:
        """Copy a qualified incidence gr_os.DataValue once into this package.

        Its authoritative native metadata is read through the public describe()
        API. The existing main-SDK Bytes export uses base64 JSON internally.
        Large values remain subject to that export's response bound.
        """
        descriptor = value.describe()
        payload = value.payload()
        try:
            if payload.representation != "bytes" or not isinstance(payload.data, bytes):
                raise TypeError("Analysis requires a qualified incidence byte carrier")
            return self.import_parts(
                spec=descriptor["spec"],
                provenance=descriptor["provenance"],
                payload=payload.data,
            )
        finally:
            payload.close()

    def decode_snapshot(self, encoded: bytes) -> NetworkSnapshot:
        result = self._native.call("snapshot.validate", payload=encoded)
        return NetworkSnapshot(
            encoded, result["snapshot_id"], result["validation_basis"]
        )


def _finalize(native, handle):
    try:
        native.call("close", handle=handle)
    except Exception:
        pass


class Network:
    def __init__(self, native: Native, description: dict):
        self._native = native
        self._handle = description["handle"]
        self._lock = threading.Lock()
        self._finalizer = weakref.finalize(self, _finalize, native, self._handle)

    def _acquire(self) -> str:
        with self._lock:
            if self._handle is None:
                raise ValueError("Network is closed")
            return self._handle

    @property
    def closed(self) -> bool:
        with self._lock:
            return self._handle is None

    def describe(self) -> dict:
        return self._native.call("describe", handle=self._acquire())

    def summary(self) -> dict:
        return self._native.call("summary", handle=self._acquire())

    def snapshot(self) -> NetworkSnapshot:
        result = self._native.call("snapshot", handle=self._acquire())
        encoded = json.dumps(
            result, ensure_ascii=False, allow_nan=False, separators=(",", ":")
        ).encode("utf-8")
        # The portable decoder deliberately reclassifies even our own export as
        # structural-only; source-derived status belongs to the retained Network.
        return NetworkSnapshot(encoded, result["snapshot_id"], "structural_only")

    def compare(self, other: Network) -> dict:
        if not isinstance(other, Network) or other._native.path != self._native.path:
            raise ValueError("Compare requires networks from the same package library")
        return self._native.call(
            "compare", left=self._acquire(), right=other._acquire()
        )

    def close(self) -> None:
        with self._lock:
            if self._handle is None:
                return
            handle, self._handle = self._handle, None
            self._finalizer.detach()
        self._native.call("close", handle=handle)

    def __enter__(self):
        self._acquire()
        return self

    def __exit__(self, *exc):
        self.close()


__all__ = ["Analysis", "Network", "NetworkSnapshot", "ModuleError", "manifest"]
