"""Transactional stores and verified immutable binary readers."""

from __future__ import annotations
import base64
import hashlib
import os
from pathlib import Path
from typing import Any, Iterable, Mapping
from ._native import Native, NativeError, _Resource, _native


def _encode_stage(records, blobs=(), required_blob_digests=()):
    transferred = []
    transferred_bytes = 0
    for blob in blobs:
        if len(transferred) >= 128:
            raise NativeError("request_limit", "Staging exceeds 128 uploaded blobs")
        if not isinstance(blob, (bytes, bytearray, memoryview)):
            raise TypeError("blobs must contain byte buffers")
        transferred_bytes += memoryview(blob).nbytes
        if transferred_bytes > 12 * 1024 * 1024:
            raise NativeError(
                "request_limit",
                "Encoded blob transfer exceeds the JSON request budget",
            )
        data = bytes(blob)
        transferred.append(
            {
                "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
                "data_base64": base64.b64encode(data).decode("ascii"),
            }
        )
    staged_records = []
    for record in records:
        if len(staged_records) >= 1000:
            raise NativeError("request_limit", "Staging exceeds 1000 record updates")
        staged_records.append(record)
    required = []
    for digest in required_blob_digests:
        if len(required) >= 100000:
            raise NativeError(
                "request_limit", "Required digest list exceeds transfer bounds"
            )
        required.append(digest)
    return {
        "records": staged_records,
        "blobs": transferred,
        "required_blob_digests": required,
    }


class Storage(_Resource):
    """Own a local native store. Use a context manager or explicit close.

    Native calls already started survive close. A call racing before native
    acquisition may fail with invalid_handle. Attribution is not authentication.
    """

    def __init__(
        self,
        path: str | os.PathLike[str] | None = None,
        *,
        backend: str = "sqlite",
        read_only: bool = False,
        native: Native | None = None,
    ):
        native = _native(native)
        parameters: dict[str, Any] = {"backend": backend, "read_only": read_only}
        if backend == "sqlite":
            if path is None:
                raise ValueError("SQLite requires an explicit path")
            parameters["path"] = str(Path(path).expanduser().resolve())
        elif path is not None:
            raise ValueError("Memory storage does not accept a filesystem path")
        result = native.call("store.open", **parameters)
        self._own(native, "store.close", result["handle"])
        self.capabilities = result["capabilities"]

    def create(self, workspace: str) -> dict:
        return self._call("store.create", workspace=workspace)

    def head(self, workspace: str) -> str:
        return self._call("store.head", workspace=workspace)["revision"]

    def snapshot(self, workspace: str, revision: str = "") -> dict:
        return self._call("store.snapshot", workspace=workspace, revision=revision)

    def transaction_capabilities(self) -> dict:
        return self._call("store.transaction_capabilities")

    def history_snapshot(
        self,
        workspace: str,
        semantic_revision: str = "",
        *,
        physical_revision: str = "",
    ) -> dict:
        return self._call(
            "store.history_snapshot",
            workspace=workspace,
            semantic_revision=semantic_revision,
            physical_revision=physical_revision,
        )

    def history_export_v1(self, workspace: str, *, physical_revision: str = "") -> dict:
        return self._call(
            "store.history_export_v1",
            workspace=workspace,
            physical_revision=physical_revision,
        )

    def begin_transaction(self, workspace: str, **options):
        from .runtime import Transaction

        return Transaction(self, workspace, **options)

    def record(self, workspace: str, id: str, version: str) -> dict:
        return self._call("store.record", workspace=workspace, id=id, version=version)

    def blob(self, workspace: str, digest: str) -> bytes:
        result = self._call("store.blob", workspace=workspace, digest=digest)
        return base64.b64decode(result["data_base64"], validate=True)

    def history(self, workspace: str, *, before: str = "", limit: int = 100) -> dict:
        return self._call(
            "store.history", workspace=workspace, before=before, limit=limit
        )

    def commit(
        self,
        workspace: str,
        records: Iterable[Mapping[str, Any]],
        *,
        expected_revision: str,
        transaction_id: str,
        actor: str,
        reason: str,
        blobs: Iterable[bytes] = (),
        required_blob_digests: Iterable[str] = (),
    ) -> dict:
        return self._call(
            "store.commit",
            workspace=workspace,
            expected_revision=expected_revision,
            transaction_id=transaction_id,
            actor=actor,
            reason=reason,
            **_encode_stage(records, blobs, required_blob_digests),
        )

    def receipt(self, workspace: str, transaction_id: str) -> dict | None:
        return self._call(
            "store.receipt", workspace=workspace, transaction_id=transaction_id
        )

    def blob_info(self, workspace: str, digest: str) -> dict:
        return self._call("store.blob.info", workspace=workspace, digest=digest)

    def open_blob(
        self, workspace: str, digest: str, *, chunk_bytes: int = 65536
    ) -> BlobReader:
        info = self._call(
            "store.blob.open",
            workspace=workspace,
            digest=digest,
            chunk_bytes=chunk_bytes,
        )
        return BlobReader(self.native, info)


class BlobReader(_Resource):
    """Pinned, verified immutable bytes; remains usable after its store closes.

    readinto writes into caller-owned contiguous writable bytes, bounded by
    chunk_bytes. Do not access/mutate that buffer concurrently with a native call.
    Cancellation is permanent for this reader and cooperative between chunks.
    """

    def __init__(self, native: Native, info: dict):
        self._own(native, "blob.close", info["handle"])
        self.metadata = {key: value for key, value in info.items() if key != "handle"}

    def cancel(self) -> None:
        self._call("blob.cancel")

    def readinto(self, buffer: Any, offset: int = 0) -> int:
        if type(offset) is not int or offset < 0 or offset > 2**64 - 1:
            raise ValueError("offset must be a uint64 integer")
        return self.native.read_blob(self._resource_handle(), offset, buffer)

    def chunks(self, *, offset: int = 0):
        buffer = bytearray(self.metadata["chunk_bytes"])
        while True:
            count = self.readinto(buffer, offset)
            if not count:
                return
            yield bytes(memoryview(buffer)[:count])
            offset += count
