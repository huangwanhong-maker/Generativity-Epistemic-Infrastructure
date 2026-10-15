"""Native typed data expressions, explicit representations, and qualified results.

Types and operator inference are validated by the same C++ library used by native
consumers. Python only builds requests and owns handles. No Python callback is
implicitly installed as a native provider. Constructors copy mutable inputs.
"""

from __future__ import annotations
import asyncio
from contextlib import ExitStack, suppress
from dataclasses import dataclass, field
import math
import operator
import struct
import time
from typing import Any, Iterable, Mapping, Sequence
from types import MappingProxyType
from ._native import Native, NativeError, _Resource, _native
from .buffers import Buffer
from .execution import Value
from .runtime import Job, Plan, Runtime, _timeout
from .storage import Storage


def _freeze(value):
    if isinstance(value, Mapping):
        return MappingProxyType({key: _freeze(item) for key, item in value.items()})
    if isinstance(value, (list, tuple)):
        return tuple(_freeze(item) for item in value)
    if value is None or type(value) in (str, bool, int, float):
        return value
    raise TypeError("Type parameters must contain JSON values")


def _thaw(value):
    if isinstance(value, Mapping):
        return {key: _thaw(item) for key, item in value.items()}
    if isinstance(value, tuple):
        return [_thaw(item) for item in value]
    return value


@dataclass(frozen=True)
class Type:
    family: str
    arguments: tuple[Type, ...] = ()
    parameters: Mapping[str, Any] = field(default_factory=dict)

    def __post_init__(self):
        object.__setattr__(self, "arguments", tuple(self.arguments))
        object.__setattr__(self, "parameters", _freeze(self.parameters))

    def _encode(self) -> dict:
        return {
            "family": self.family,
            "arguments": [v._encode() for v in self.arguments],
            "parameters": _thaw(self.parameters),
        }

    @staticmethod
    def _decode(value: dict) -> Type:
        return Type(
            value["family"],
            tuple(Type._decode(v) for v in value["arguments"]),
            value["parameters"],
        )


@dataclass(frozen=True)
class ValueSpec:
    type: Type
    representation: str

    def _encode(self) -> dict:
        return {"type": self.type._encode(), "representation": self.representation}

    @staticmethod
    def _decode(value: dict) -> ValueSpec:
        return ValueSpec(Type._decode(value["type"]), value["representation"])


class DataValue(_Resource):
    """An immutable native value. payload() returns a separately owned export."""

    def __init__(self, native: Native, result: dict):
        self._own(native, "data.value.close", result["handle"])
        self._spec = ValueSpec._decode(result["spec"])
        self.provenance = result["provenance"]

    @property
    def spec(self) -> ValueSpec:
        return self._spec

    def retain(self) -> DataValue:
        return DataValue(self.native, self._call("data.value.retain"))

    def payload(self) -> Value:
        """Caller owns the returned buffer handles independently of this value."""
        return Value._decode(self._call("data.value.payload"), self.native)

    def tolist(self) -> list | int | float | complex | None:
        """Explicit copying conversion of supported scalars/columns into Python."""
        result = self._call("data.value.items")
        if isinstance(result, dict) and "complex_values" in result:
            return [
                None if value is None else complex(*value)
                for value in result["complex_values"]
            ]
        if isinstance(result, dict) and "complex_value" in result:
            return complex(*result["complex_value"])
        return result

    def inspect(self) -> dict:
        """Explicit bounded decoding of a qualified relational representation."""
        return self._call("data.value.inspect")

    def numpy(self, *, copy: bool = False):
        """Return a NumPy view of a nonnullable supported numeric column.

        Nullable columns require an explicit validity policy and are rejected.
        copy=False pins the native readonly exporter; copy=True returns a caller
        owned writable array and works without the optional CPython exporter.
        """
        if type(copy) is not bool:
            raise TypeError("copy must be boolean")
        import numpy as np

        # Conversion policy comes from the immutable native value, never from
        # a caller's Python-side descriptor or mutated convenience cache.
        logical = ValueSpec._decode(self._call("data.value.describe")["spec"]).type
        if logical.family != "gr.data.column@1" or logical.parameters.get("nullable"):
            raise ValueError("numpy() requires an explicitly nonnullable column")
        formats = {
            "gr.data.int64@1": "<i8",
            "gr.data.float64@1": "<f8",
            "gr.data.boolean@1": "u1",
            "gr.numeric.uint64@1": "<u8",
            "gr.numeric.complex@1": "<c16",
        }
        dtype = formats.get(logical.arguments[0].family)
        if dtype is None:
            raise ValueError("No NumPy conversion is registered for this element type")
        payload = self.payload()
        try:
            source = payload.data.buffers["values"]
            if copy:
                result = np.frombuffer(source.to_bytes(), dtype=dtype).copy()
            else:
                if not source.contiguous:
                    raise ValueError("Strided columns require numpy(copy=True)")
                view = source.memoryview()
                # frombuffer consumes the buffer protocol and retains its owner.
                # Explicit dtype handles boolean/float encodings opaque to PEP3118.
                try:
                    result = np.frombuffer(view, dtype=dtype)
                finally:
                    view.release()
        finally:
            payload.close()
        if logical.arguments[0].family == "gr.data.boolean@1":
            result = result.view(np.bool_)
        return result


class Expr(_Resource):
    def __init__(self, native: Native, result: dict):
        self._own(native, "data.expr.close", result["handle"])
        self.spec = ValueSpec._decode(result["spec"])
        self.description = result["expression"]

    def retain(self) -> Expr:
        return Expr(self.native, self._call("data.expr.retain"))

    def compile(self, *, bindings: Mapping[str, DataValue] | None = None) -> Program:
        return compile([self], bindings=bindings)

    def function(self) -> Function:
        return Function([self])


class _PlanView(Plan):
    """Borrowing adapter to the existing runtime Plan API; retains its Program."""

    def __init__(self, program: Program):
        self.program = program
        self.native = program.native
        self.description = program._plan_description

    def _resource_handle(self) -> str:
        self.program._resource_handle()
        return self.program._plan_handle

    @property
    def closed(self) -> bool:
        return self.program.closed

    def close(self) -> None:
        self.program.close()


@dataclass
class DataResult:
    id: str
    status: str
    outputs: list[DataValue]
    publication: Any
    error: Any
    diagnostics: dict
    recovery: DataJob | None = field(default=None, repr=False)

    @staticmethod
    def _decode(native: Native, result: dict) -> DataResult:
        values = []
        try:
            for value in result["outputs"]:
                values.append(DataValue(native, value))
            return DataResult(
                result["id"],
                result["status"],
                values,
                result["publication"],
                result["error"],
                result["diagnostics"],
            )
        except BaseException:
            for value in values:
                value.close()
            for value in result["outputs"][len(values) :]:
                with suppress(Exception):
                    native.call("data.value.close", handle=value["handle"])
            raise

    def raise_for_status(self) -> DataResult:
        if self.status != "succeeded":
            error = self.error or {}
            failure = NativeError(
                error.get("code", self.status),
                error.get("message", "Data job failed"),
                error.get("details", {}),
            )
            failure.diagnostics = self.diagnostics
            failure.result = self
            raise failure
        return self

    def recover_publication(self, *, retry: bool = False) -> dict:
        if self.recovery is None:
            raise ValueError("Use the original DataJob for recovery")
        return self.recovery.recover_publication(retry=retry)

    def close(self) -> None:
        for value in self.outputs:
            value.close()
        if self.recovery is not None:
            self.recovery.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


class DataJob:
    """Owns an ordinary Runtime Job plus an independent native Program lease."""

    def __init__(self, program: Program, job: Job):
        self.program, self.job = program, job

    def status(self) -> dict:
        return self.job.status()

    def cancel(self) -> dict:
        return self.job.cancel()

    def recover_publication(self, *, retry: bool = False) -> dict:
        return self.job.recover_publication(retry=retry)

    def _poll(self, milliseconds: int) -> dict:
        return self.program._call(
            "data.result", job=self.job._resource_handle(), timeout_ms=milliseconds
        )

    def wait(self, timeout: float | None = None) -> DataResult | None:
        deadline = _timeout(timeout)
        while True:
            remaining = (
                None if deadline is None else max(0, deadline - time.monotonic())
            )
            response = self._poll(
                100 if remaining is None else int(min(0.1, remaining) * 1000)
            )
            if response["ready"]:
                return DataResult._decode(self.program.native, response["result"])
            if deadline is not None and time.monotonic() >= deadline:
                return None

    def result(self, timeout: float | None = None) -> DataResult:
        result = self.wait(timeout)
        if result is None:
            raise TimeoutError("Data job is pending; no cancellation was requested")
        return result

    async def wait_async(self, timeout: float | None = None) -> DataResult | None:
        deadline = _timeout(timeout)
        try:
            while True:
                response = self._poll(0)
                if response["ready"]:
                    return DataResult._decode(self.program.native, response["result"])
                if deadline is not None and time.monotonic() >= deadline:
                    return None
                await asyncio.sleep(0.01)
        except asyncio.CancelledError:
            with suppress(Exception):
                self.cancel()
            raise

    def __await__(self):
        return self.wait_async().__await__()

    def close(self) -> None:
        try:
            self.job.close()
        finally:
            self.program.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


class Program(_Resource):
    def __init__(self, native: Native, result: dict):
        self._own(native, "data.program.close", result["handle"])
        self.description = result["program"]
        self._plan_handle = result["plan"]["handle"]
        self._plan_description = result["plan"]["plan"]

    @property
    def plan(self) -> Plan:
        return _PlanView(self)

    def retain(self) -> Program:
        return Program(self.native, self._call("data.program.retain"))

    def submit(self, runtime: Runtime, **options: Any) -> DataJob:
        if runtime.native is not self.native:
            raise ValueError("Program and Runtime must use the same Native instance")
        retained = self.retain()
        try:
            return DataJob(retained, runtime.submit(retained.plan, **options))
        except BaseException:
            retained.close()
            raise

    def run(self, runtime: Runtime, **options: Any) -> DataResult:
        job = self.submit(runtime, **options)
        retained = False
        try:
            result = job.result()
            if result.status == "outcome_unknown":
                result.recovery = job
                retained = True
            return result
        except BaseException:
            with suppress(Exception):
                job.cancel()
            raise
        finally:
            if not retained:
                job.close()


def compile(
    outputs: Sequence[Expr], *, bindings: Mapping[str, DataValue] | None = None
) -> Program:
    if not outputs or len(outputs) > 4096:
        raise ValueError("Compile needs 1..4096 expression outputs")
    native = outputs[0].native
    if any(value.native is not native for value in outputs):
        raise ValueError("Expressions must use the same Native instance")
    bindings = dict(bindings or {})
    if any(value.native is not native for value in bindings.values()):
        raise ValueError("Bindings must use the same Native instance")
    return Program(
        native,
        native.call(
            "data.compile",
            outputs=[v._resource_handle() for v in outputs],
            bindings={
                name: value._resource_handle() for name, value in bindings.items()
            },
        ),
    )


class Function(_Resource):
    """Immutable native expression template with named inputs and composition."""

    def __init__(self, outputs: Sequence[Expr]):
        if not outputs or len(outputs) > 4096:
            raise ValueError("Function needs 1..4096 expression outputs")
        native = outputs[0].native
        if any(value.native is not native for value in outputs):
            raise ValueError("Function outputs must use the same Native instance")
        self._initialize(
            native,
            native.call(
                "data.function.create",
                outputs=[value._resource_handle() for value in outputs],
            ),
        )

    def _initialize(self, native, result):
        self._own(native, "data.function.close", result["handle"])
        self.description = result["function"]
        self.inputs = MappingProxyType(
            {name: ValueSpec._decode(spec) for name, spec in result["inputs"].items()}
        )

    def retain(self) -> Function:
        value = Function.__new__(Function)
        value._initialize(self.native, self._call("data.function.retain"))
        return value

    def _bindings(self, bindings):
        if any(value.native is not self.native for value in bindings.values()):
            raise ValueError("Function bindings must use the same Native instance")
        return {name: value._resource_handle() for name, value in bindings.items()}

    def bind(self, bindings: Mapping[str, DataValue] | None = None) -> Program:
        return Program(
            self.native,
            self._call("data.function.bind", bindings=self._bindings(bindings or {})),
        )

    def instantiate(self, bindings: Mapping[str, Expr]) -> tuple[Expr, ...]:
        result = self._call(
            "data.function.instantiate", bindings=self._bindings(bindings)
        )
        outputs = []
        try:
            for value in result["outputs"]:
                outputs.append(Expr(self.native, value))
            return tuple(outputs)
        except BaseException:
            for value in outputs:
                value.close()
            for value in result["outputs"][len(outputs) :]:
                with suppress(Exception):
                    self.native.call("data.expr.close", handle=value["handle"])
            raise


class Context(_Resource):
    """A sealed native logical library with explicitly selected extensions."""

    def __init__(
        self,
        *,
        modules: Sequence[str] = ("gr.data.standard@1",),
        native: Native | None = None,
        validation_cache: Mapping | None = None,
        relational_cache: Mapping | None = None,
    ):
        native = _native(native)
        if "data_language" not in native.capabilities.get("features", []):
            raise RuntimeError(
                "Layer 3 requires a GR-OS 0.5 build with GR_BUILD_DATA enabled"
            )
        parameters = {"modules": list(modules)}
        if validation_cache is not None:
            parameters["validation_cache"] = dict(validation_cache)
        if relational_cache is not None:
            parameters["relational_cache"] = dict(relational_cache)
        result = native.call("data.open", **parameters)
        self._own(native, "data.close", result["handle"])
        self.capabilities = result["capabilities"]

    def validation_statistics(self) -> dict:
        return self._call("data.validation.statistics")

    def clear_validation_cache(self) -> dict:
        return self._call("data.validation.clear")

    def numeric_capabilities(self) -> dict:
        return self._call("data.numeric.capabilities")

    def relational_statistics(self) -> dict:
        return self._call("data.relational.statistics")

    def clear_relational_cache(self) -> dict:
        return self._call("data.relational.clear")

    def inspect(self, value: DataValue) -> dict:
        if value.native is not self.native:
            raise ValueError("Value and Context must use the same Native instance")
        return self._call("data.relational.inspect", value=value._resource_handle())

    def import_value(
        self, spec: ValueSpec, payload: Value, *, provenance: Mapping | None = None
    ) -> DataValue:
        return DataValue(
            self.native,
            self._call(
                "data.import",
                value={
                    "spec": spec._encode(),
                    "payload": payload._encode(self.native),
                    "provenance": dict(provenance or {}),
                },
            ),
        )

    def literal(self, value: DataValue) -> Expr:
        if value.native is not self.native:
            raise ValueError("Value and Context must use the same Native instance")
        return Expr(
            self.native, self._call("data.literal", value=value._resource_handle())
        )

    def input(self, name: str, spec: ValueSpec) -> Expr:
        return Expr(
            self.native, self._call("data.input", name=name, spec=spec._encode())
        )

    def call(
        self,
        operation: str,
        inputs: Sequence[Expr],
        *,
        attributes: Mapping | None = None,
        **options: Any,
    ) -> tuple[Expr, ...]:
        if any(value.native is not self.native for value in inputs):
            raise ValueError(
                "Expressions and Context must use the same Native instance"
            )
        result = self._call(
            "data.call",
            operation=operation,
            inputs=[v._resource_handle() for v in inputs],
            attributes=dict(attributes or {}),
            options=options,
        )
        outputs = []
        try:
            for value in result["outputs"]:
                outputs.append(Expr(self.native, value))
            return tuple(outputs)
        except BaseException:
            for value in outputs:
                value.close()
            for value in result["outputs"][len(outputs) :]:
                with suppress(Exception):
                    self.native.call("data.expr.close", handle=value["handle"])
            raise

    def apply(
        self,
        operation: str,
        inputs: Sequence[Expr],
        *,
        attributes: Mapping | None = None,
        **options: Any,
    ) -> Expr:
        outputs = self.call(operation, inputs, attributes=attributes, **options)
        if len(outputs) != 1:
            for value in outputs:
                value.close()
            raise ValueError(
                "apply requires exactly one output; use call for multiple outputs"
            )
        return outputs[0]

    def scalar(
        self, value: int | float | complex, *, dtype: str = "int64"
    ) -> DataValue:
        if isinstance(value, bool):
            raise TypeError("A numeric scalar cannot be boolean")
        if dtype in ("int64", "uint64"):
            value = operator.index(value)
            lower, upper = (-(2**63), 2**63) if dtype == "int64" else (0, 2**64)
            if not lower <= value < upper:
                raise OverflowError("Value is outside " + dtype)
            raw_bytes = struct.pack("<q" if dtype == "int64" else "<Q", value)
        elif dtype == "float64":
            value = float(value)
            if not math.isfinite(value):
                raise ValueError("Finite float64 is required")
            raw_bytes = struct.pack("<d", value)
        elif dtype == "complex128":
            value = complex(value)
            if not math.isfinite(value.real) or not math.isfinite(value.imag):
                raise ValueError("Finite complex128 is required")
            raw_bytes = struct.pack("<dd", value.real, value.imag)
        else:
            raise ValueError("Unsupported numeric dtype")
        with Buffer(raw_bytes, native=self.native) as raw:
            return DataValue(
                self.native,
                self._call(
                    "data.create",
                    kind={
                        "int64": "scalar_i64",
                        "uint64": "scalar_u64",
                        "float64": "scalar_f64",
                        "complex128": "scalar_c128",
                    }[dtype],
                    values=raw._resource_handle(),
                ),
            )

    def column(
        self, values: Iterable, *, dtype: str = "int64", nullable: bool | None = None
    ) -> DataValue:
        if dtype not in ("int64", "uint64", "bool", "float64", "complex128"):
            raise ValueError(
                "Supported column dtypes are int64, uint64, bool, float64 and complex128"
            )
        if nullable is not None and type(nullable) is not bool:
            raise TypeError("nullable must be boolean or None")
        raw, validity, missing = bytearray(), bytearray(), False
        for item in values:
            if len(validity) >= 1024 * 1024:
                raise ValueError("Column exceeds 1,048,576 elements")
            valid = item is not None
            validity.append(int(valid))
            if not valid:
                missing = True
                item = False if dtype == "bool" else 0
            if dtype in ("int64", "uint64"):
                if isinstance(item, bool):
                    raise TypeError("Integer column cannot contain booleans")
                raw.extend(
                    struct.pack(
                        "<q" if dtype == "int64" else "<Q", operator.index(item)
                    )
                )
            elif dtype == "bool":
                if type(item) is not bool:
                    raise TypeError("Boolean columns require bool or None")
                raw.append(int(item))
            elif dtype == "complex128":
                if isinstance(item, bool):
                    raise TypeError("Complex column cannot contain booleans")
                number = complex(item)
                if not math.isfinite(number.real) or not math.isfinite(number.imag):
                    raise ValueError("Finite complex128 is required")
                raw.extend(struct.pack("<dd", number.real, number.imag))
            else:
                if isinstance(item, bool) or not isinstance(item, (float, int)):
                    raise TypeError("Float64 columns require numbers or None")
                number = float(item)
                if not math.isfinite(number):
                    raise ValueError(
                        "This float64 representation requires finite values"
                    )
                raw.extend(struct.pack("<d", number))
        if missing and nullable is False:
            raise ValueError("Missing values require a nullable column")
        with ExitStack() as stack:
            buffer = stack.enter_context(Buffer(raw, native=self.native))
            parameters = {"values": buffer._resource_handle()}
            if nullable is True or missing:
                mask = stack.enter_context(Buffer(validity, native=self.native))
                parameters["validity"] = mask._resource_handle()
            return DataValue(
                self.native,
                self._call(
                    "data.create",
                    kind={
                        "int64": "column_i64",
                        "uint64": "column_u64",
                        "bool": "column_bool",
                        "float64": "column_f64",
                        "complex128": "column_c128",
                    }[dtype],
                    **parameters,
                ),
            )

    def from_numpy(self, array) -> DataValue:
        """Explicit copy-in of a 1D int64/uint64/bool/float64/complex128 array."""
        import numpy as np

        if not isinstance(array, np.ndarray) or array.ndim != 1:
            raise TypeError("Expected a one-dimensional numpy.ndarray")
        if array.size > 1024 * 1024:
            raise ValueError("Column exceeds 1,048,576 elements")
        if array.dtype.kind == "i" and array.dtype.itemsize == 8:
            dtype, kind = np.dtype("<i8"), "column_i64"
        elif array.dtype.kind == "u" and array.dtype.itemsize == 8:
            dtype, kind = np.dtype("<u8"), "column_u64"
        elif array.dtype.kind == "c" and array.dtype.itemsize == 16:
            dtype, kind = np.dtype("<c16"), "column_c128"
        elif array.dtype.kind == "f" and array.dtype.itemsize == 8:
            dtype, kind = np.dtype("<f8"), "column_f64"
        elif array.dtype.kind == "b":
            dtype, kind = np.dtype("u1"), "column_bool"
        else:
            raise TypeError(
                "Explicit conversion to int64, uint64, float64, complex128 or bool is required"
            )
        # Normalize byte order/layout explicitly, then native construction validates.
        raw = np.asarray(array, dtype=dtype, order="C").tobytes(order="C")
        with Buffer(raw, native=self.native) as buffer:
            return DataValue(
                self.native,
                self._call("data.create", kind=kind, values=buffer._resource_handle()),
            )

    def table(self, columns: Mapping[str, DataValue]) -> DataValue:
        if any(value.native is not self.native for value in columns.values()):
            raise ValueError("All columns must use the same Native instance")
        return DataValue(
            self.native,
            self._call(
                "data.create",
                kind="table",
                columns=[
                    {"name": name, "value": value._resource_handle()}
                    for name, value in columns.items()
                ],
            ),
        )

    def csr(
        self,
        rows: int,
        columns: int,
        indptr: Sequence[int],
        indices: Sequence[int],
        values: Sequence[int],
    ) -> DataValue:
        with ExitStack() as stack:
            parameters = {"rows": rows, "columns": columns}
            for name, items in (
                ("indptr", indptr),
                ("indices", indices),
                ("values", values),
            ):
                limit = 1024 * 1024 + int(name == "indptr")
                raw = bytearray()
                for index, item in enumerate(items):
                    if index >= limit:
                        raise ValueError("CSR input exceeds element limit")
                    if isinstance(item, bool):
                        raise TypeError("CSR items must be int64")
                    raw.extend(struct.pack("<q", operator.index(item)))
                buffer = stack.enter_context(Buffer(raw, native=self.native))
                parameters[name] = buffer._resource_handle()
            return DataValue(
                self.native, self._call("data.create", kind="csr_i64", **parameters)
            )

    def history(
        self,
        store: Storage,
        workspace: str,
        *,
        semantic_revision: str = "",
        physical_revision: str = "",
    ) -> DataValue:
        if store.native is not self.native:
            raise ValueError("Store and Context must use the same Native instance")
        return DataValue(
            self.native,
            self._call(
                "data.history",
                store=store._resource_handle(),
                workspace=workspace,
                semantic_revision=semantic_revision,
                physical_revision=physical_revision,
            ),
        )
