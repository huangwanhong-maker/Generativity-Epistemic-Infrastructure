"""Explicit typed execution and pinned byte convenience operations."""

from __future__ import annotations
import base64
import json
from dataclasses import dataclass
from typing import Any, Iterable
from ._native import Native, NativeError, _native
from .buffers import BufferSet


def _check_options(options: dict[str, Any]) -> None:
    counts = {
        "max_threads",
        "max_batch_items",
        "max_input_bytes",
        "max_output_bytes",
        "deadline_ms",
    }
    allowed = counts | {"device_id", "cancelled"}
    for key, value in options.items():
        if key not in allowed:
            raise NativeError("invalid_request", f"Unknown execution option: {key}")
        if key in counts and (type(value) is not int or value < 0 or value > 2**64 - 1):
            raise NativeError("invalid_request", f"{key} must be a uint64 integer")
        if key == "cancelled" and type(value) is not bool:
            raise NativeError("invalid_request", "cancelled must be a boolean")
        if key == "device_id" and (not isinstance(value, str) or "\0" in value):
            raise NativeError(
                "invalid_request", "device_id must be a string without NUL"
            )
    if options.get("max_threads", 1) == 0:
        raise NativeError("execution_resource_limit", "max_threads must be positive")
    if options.get("deadline_ms", 0) > 86400000:
        raise NativeError("invalid_request", "deadline_ms exceeds one day")


@dataclass(frozen=True)
class ExecutionResult:
    outputs: list[bytes]
    metadata: dict[str, Any]


def devices(*, native: Native | None = None) -> Any:
    return _native(native).call("execution.devices")


def execution_capabilities(
    backend: str = "cpu.sequential", *, native: Native | None = None
) -> dict:
    return _native(native).call("execution.capabilities", backend=backend)


def execute(
    operation: str,
    inputs: Iterable[bytes],
    *,
    version: str = "1",
    backend: str = "cpu.sequential",
    native: Native | None = None,
    **options: Any,
) -> ExecutionResult:
    """Run a bounded synchronous batch of identity@1 or sha256@1.

    Inputs/results copy through base64 at this convenience boundary. This is not
    zero-copy scientific data interchange. Live cooperative cancellation and
    trusted operation registration are native C++ interfaces; JSON supports an
    initial ``cancelled`` flag and a relative ``deadline_ms`` only.
    """
    _check_options(options)
    if isinstance(inputs, (bytes, bytearray, memoryview)):
        raise TypeError("inputs must be an iterable of byte buffers, not one buffer")
    encoded = []
    used = 0
    for data in inputs:
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError("inputs must contain byte buffers")
        batch_limit = options.get("max_batch_items", 65536)
        if type(batch_limit) is int and len(encoded) >= batch_limit:
            raise NativeError(
                "execution_batch_limit", "Batch item count exceeds max_batch_items"
            )
        used += memoryview(data).nbytes
        limit = options.get("max_input_bytes", 64 * 1024 * 1024)
        if type(limit) is int and used > limit:
            raise NativeError(
                "execution_input_limit", "Input exceeds configured byte limit"
            )
        if used > 12 * 1024 * 1024:
            raise NativeError(
                "request_limit", "Encoded inputs exceed JSON request budget"
            )
        encoded.append(base64.b64encode(data).decode("ascii"))
    result = _native(native).call(
        "execution.execute",
        operation=operation,
        version=version,
        backend=backend,
        inputs_base64=encoded,
        options=options,
    )
    return ExecutionResult(
        [base64.b64decode(data, validate=True) for data in result["outputs_base64"]],
        result["metadata"],
    )


@dataclass(frozen=True)
class Value:
    type: str
    data: Any
    representation: str = "json"

    @staticmethod
    def bytes(data: bytes, type: str = "gr.bytes@1") -> Value:
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise TypeError("Value.bytes requires a byte buffer")
        return Value(type, bytes(data), "bytes")

    @staticmethod
    def json(type: str, data: Any) -> Value:
        return Value(type, data, "json")

    @staticmethod
    def buffers(type: str, data: BufferSet) -> Value:
        if not isinstance(data, BufferSet):
            raise TypeError("Expected BufferSet")
        return Value(type, data, "buffers")

    @staticmethod
    def _decode(value: dict, native: Native | None = None) -> Value:
        if value["representation"] == "bytes":
            return Value.bytes(
                base64.b64decode(value["data_base64"], validate=True), value["type"]
            )
        if value["representation"] == "buffers":
            if native is None:
                raise ValueError("Native is required for buffer result ownership")
            return Value.buffers(
                value["type"], BufferSet._decode(native, value["data"])
            )
        return Value.json(value["type"], value["data"])

    def close(self) -> None:
        if self.representation == "buffers":
            self.data.close()

    def _encode(self, native: Native | None = None) -> dict:
        if self.representation == "buffers":
            return {
                "type": self.type,
                "representation": "buffers",
                "data": self.data._encode(native),
            }
        if self.representation == "bytes":
            return {
                "type": self.type,
                "representation": "bytes",
                "data_base64": base64.b64encode(self.data).decode("ascii"),
            }
        if self.representation == "json":
            return {"type": self.type, "representation": "json", "data": self.data}
        raise ValueError("Value representation must be json or bytes")


@dataclass(frozen=True)
class TypedExecutionResult:
    outputs: list[list[Value]]
    metadata: dict[str, Any]


def contracts(*, native: Native | None = None) -> dict:
    return _native(native).call("execution.contracts")


def execute_typed(
    operation: str,
    implementation: str,
    invocations: Iterable[Iterable[Value]],
    *,
    backend: str = "cpu.sequential",
    native: Native | None = None,
    **options: Any,
) -> TypedExecutionResult:
    _check_options(options)
    native = _native(native)
    encoded = []
    used = 0
    limit = options.get("max_batch_items", 65536)
    byte_limit = options.get("max_input_bytes", 64 * 1024 * 1024)
    for arguments in invocations:
        if type(limit) is int and len(encoded) >= limit:
            raise NativeError(
                "execution_batch_limit", "Batch item count exceeds max_batch_items"
            )
        row = []
        for value in arguments:
            if len(row) >= 1024:
                raise NativeError("request_limit", "Invocation arity exceeds 1024")
            if not isinstance(value, Value):
                raise TypeError("Typed arguments must be Value objects")
            size = (
                sum(
                    max(b.nbytes, b.retained_bytes) for b in value.data.buffers.values()
                )
                if value.representation == "buffers"
                else (
                    memoryview(value.data).nbytes
                    if value.representation == "bytes"
                    else len(
                        json.dumps(
                            value.data,
                            ensure_ascii=False,
                            allow_nan=False,
                            separators=(",", ":"),
                        ).encode("utf-8")
                    )
                )
            )
            used += size
            if type(byte_limit) is int and byte_limit >= 0 and used > byte_limit:
                raise NativeError(
                    "execution_input_limit", "Input exceeds configured byte limit"
                )
            if used > 64 * 1024 * 1024:
                raise NativeError(
                    "request_limit", "Typed input exceeds the JSON transport budget"
                )
            row.append(value._encode(native))
        encoded.append(row)
    result = _native(native).call(
        "execution.execute_typed",
        operation=operation,
        implementation=implementation,
        invocations=encoded,
        backend=backend,
        options=options,
    )
    outputs = [
        [Value._decode(value, native) for value in invocation]
        for invocation in result["outputs"]
    ]
    return TypedExecutionResult(outputs, result["metadata"])
