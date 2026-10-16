"""Package-specific synchronous facade over the installed GR native language.

This library has its own native state; gr_os opaque handles are not accepted.
Pass ordinary int64 sequences explicitly. All input data is copied.
"""

import ctypes
from dataclasses import dataclass
import json
import operator
import os
from pathlib import Path

__version__ = "0.2.0"


def manifest():
    """Read package metadata without loading the native library."""
    from importlib.resources import files

    return json.loads(
        files(__package__).joinpath("manifest.json").read_text(encoding="utf-8")
    )


class ExtensionError(RuntimeError):
    def __init__(self, report):
        error = report.get("error") or {}
        super().__init__(error.get("message", "Native extension failed"))
        self.code = error.get("code", report.get("status", "extension_failure"))
        self.details = error.get("details", {})
        self.report = report


@dataclass(frozen=True)
class Result:
    values: list[int]
    report: dict


class Diagonal:
    def __init__(self, library=None):
        selected = (
            library if library is not None else os.environ.get("GR_DIAGONAL_LIBRARY")
        )
        if not selected:
            raise ValueError("Select the separately installed GR_DIAGONAL_LIBRARY")
        self.path = Path(selected).resolve(strict=True)
        self.library = ctypes.CDLL(str(self.path))
        self.library.gr_diagonal_abi_version.restype = ctypes.c_uint32
        if self.library.gr_diagonal_abi_version() != 1:
            raise RuntimeError("Unsupported package-specific C ABI")
        self.library.gr_diagonal_apply_i64_v1.argtypes = [
            ctypes.POINTER(ctypes.c_int64),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_int64),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_int64),
            ctypes.c_size_t,
            ctypes.c_uint64,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self.library.gr_diagonal_apply_i64_v1.restype = ctypes.c_int
        self.library.gr_diagonal_free.argtypes = [ctypes.c_void_p]
        self.library.gr_diagonal_free.restype = None

    @staticmethod
    def _integers(values):
        result = []
        for value in values:
            if len(result) >= 1024 * 1024:
                raise ValueError("Extension input exceeds 1,048,576 elements")
            if isinstance(value, bool):
                raise TypeError("int64 input cannot be boolean")
            value = operator.index(value)
            if not -(2**63) <= value < 2**63:
                raise OverflowError("Value is outside int64")
            result.append(value)
        return (ctypes.c_int64 * len(result))(*result)

    def apply(self, diagonal, vector, *, timeout_ms=0):
        if type(timeout_ms) is not int or not 0 <= timeout_ms <= 60000:
            raise ValueError(
                "timeout_ms must be an integer in 0..60000; zero means no deadline"
            )
        left, right = self._integers(diagonal), self._integers(vector)
        output = (ctypes.c_int64 * len(left))()
        allocation, size = ctypes.c_void_p(), ctypes.c_size_t()
        status = self.library.gr_diagonal_apply_i64_v1(
            left,
            len(left),
            right,
            len(right),
            output,
            len(output),
            timeout_ms,
            ctypes.byref(allocation),
            ctypes.byref(size),
        )
        try:
            if status == 2 or not allocation.value:
                raise RuntimeError("Package C transport/allocation failure")
            report = json.loads(ctypes.string_at(allocation, size.value))
        finally:
            if allocation.value:
                self.library.gr_diagonal_free(allocation)
        if status or not report.get("ok"):
            raise ExtensionError(report)
        return Result(list(output), report)
