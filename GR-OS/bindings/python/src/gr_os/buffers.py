"""Immutable owned host buffers and validated physical views.

Input exporters are copied. memoryview() is genuinely readonly and retains native
ownership even after this handle closes. It requires the optional native CPython
exporter; to_bytes/readinto are the portable copying fallback.
"""

from __future__ import annotations
from typing import Any, Mapping
from ._native import Native, _Resource, _native


class Buffer(_Resource):
    def __init__(
        self,
        data: Any,
        *,
        layout: Mapping[str, Any] | None = None,
        native: Native | None = None,
    ):
        native = _native(native)
        result = native.import_buffer(data, dict(layout or {}))
        self._initialize(native, result)

    def _initialize(self, native: Native, result: dict) -> None:
        self._own(native, "buffer.close", result["handle"])
        self.layout = result["layout"]
        self.nbytes = result["logical_bytes"]
        self.retained_bytes = result["retained_bytes"]
        self.contiguous = result["contiguous"]

    @classmethod
    def _from_native(cls, native: Native, result: dict) -> Buffer:
        value = cls.__new__(cls)
        value._initialize(native, result)
        return value

    def retain(self) -> Buffer:
        return self._from_native(self.native, self._call("buffer.retain"))

    def slice(self, start: int, count: int, step: int = 1, *, axis: int = 0) -> Buffer:
        return self._from_native(
            self.native,
            self._call("buffer.slice", axis=axis, start=start, count=count, step=step),
        )

    def copy_contiguous(self) -> Buffer:
        return self._from_native(self.native, self._call("buffer.copy_contiguous"))

    def readinto(self, destination: Any, offset: int = 0) -> int:
        return self.native.read_buffer(self._resource_handle(), offset, destination)

    def to_bytes(self) -> bytes:
        if not self.contiguous:
            with self.copy_contiguous() as copy:
                return copy.to_bytes()
        output = bytearray(self.nbytes)
        self.readinto(output)
        return bytes(output)

    def memoryview(self) -> memoryview:
        return self.native.export_buffer(self._resource_handle())

    def digest(self) -> str:
        return self._call("buffer.digest")["digest"]


class BufferSet:
    """A named carrier; constructor borrows Buffer objects, decoded results own them.

    close() explicitly closes every contained Buffer. A Value/plan copies native
    ownership at admission, so later Python handle closure cannot invalidate it.
    """

    def __init__(
        self, buffers: Mapping[str, Buffer], metadata: Mapping[str, Any] | None = None
    ):
        self.buffers = dict(buffers)
        self.metadata = dict(metadata or {})
        if not all(isinstance(v, Buffer) for v in self.buffers.values()):
            raise TypeError("BufferSet values must be Buffer handles")

    def _encode(self, native: Native | None = None) -> dict:
        identities = {id(v.native) for v in self.buffers.values()}
        if len(identities) > 1 or (
            native is not None
            and any(v.native is not native for v in self.buffers.values())
        ):
            raise ValueError(
                "All buffers and the runtime must use the same Native instance"
            )
        return {
            "buffers": [
                {"name": name, "handle": value._resource_handle()}
                for name, value in self.buffers.items()
            ],
            "metadata": self.metadata,
        }

    @classmethod
    def _decode(cls, native: Native, value: dict) -> BufferSet:
        return cls(
            {
                entry["name"]: Buffer._from_native(native, entry)
                for entry in value["buffers"]
            },
            value["metadata"],
        )

    def close(self) -> None:
        for value in self.buffers.values():
            value.close()

    def __enter__(self) -> BufferSet:
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()
