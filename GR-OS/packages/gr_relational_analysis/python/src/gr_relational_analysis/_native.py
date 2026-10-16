"""Private package-specific transport. No main-SDK opaque handles cross here."""

import ctypes
import json
import os
from pathlib import Path


class ModuleError(RuntimeError):
    def __init__(self, error):
        super().__init__(error.get("message", "Native package failure"))
        self.code = error.get("code", "module_failure")
        self.details = error.get("details", {})


class Native:
    def __init__(self, library=None):
        selected = (
            library if library is not None else os.environ.get("GR_ANALYSIS_LIBRARY")
        )
        if not selected:
            raise ValueError("Select the separately installed GR_ANALYSIS_LIBRARY")
        self.path = Path(selected).resolve(strict=True)
        self.library = ctypes.CDLL(str(self.path))
        abi = self.library.gr_analysis_abi_version
        abi.argtypes = []
        abi.restype = ctypes.c_uint32
        if abi() != 1:
            raise RuntimeError("Unsupported package-specific C ABI")
        self._invoke = self.library.gr_analysis_call_v1
        self._invoke.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self._invoke.restype = ctypes.c_int
        self._free = self.library.gr_analysis_free
        self._free.argtypes = [ctypes.c_void_p]
        self._free.restype = None

    def call(self, op, *, payload=b"", **fields):
        if not isinstance(payload, bytes):
            raise TypeError("Raw payload must be immutable bytes")
        if len(payload) > 64 * 1024 * 1024:
            raise ValueError("Raw payload exceeds 64 MiB")
        request = json.dumps(
            {"op": op, **fields},
            ensure_ascii=False,
            allow_nan=False,
            separators=(",", ":"),
        ).encode("utf-8")
        if len(request) > 1024 * 1024:
            raise ValueError("Request metadata exceeds 1 MiB")
        response, size = ctypes.c_void_p(), ctypes.c_size_t()
        status = self._invoke(
            request,
            len(request),
            payload,
            len(payload),
            ctypes.byref(response),
            ctypes.byref(size),
        )
        try:
            if status == 2 or not response.value:
                raise RuntimeError("Package transport/allocation failure")
            if size.value > 65 * 1024 * 1024:
                raise RuntimeError("Package response exceeded declared limit")
            result = json.loads(ctypes.string_at(response, size.value))
        finally:
            if response.value:
                self._free(response)
        if status or not result.get("ok"):
            raise ModuleError(result.get("error", {}))
        return result["result"]
