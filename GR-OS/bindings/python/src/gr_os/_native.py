"""Versioned native transport and owned-resource lifecycle."""

from __future__ import annotations
import ctypes
import importlib.util
import json
import os
import sys
from pathlib import Path
from threading import Lock
from typing import Any
import weakref


class NativeError(RuntimeError):
    """A structured error returned by the native library."""

    def __init__(self, code: str, message: str, details: Any = None):
        super().__init__(message)
        self.code = code
        self.details = details if details is not None else {}


class Native:
    """An explicitly selected native library; requests are synchronous.

    ctypes releases the GIL during each native call. Native storage and execution
    own their locking. Do not unload a library while its handles are in use.
    """

    def __init__(self, library: str | os.PathLike[str] | None = None):
        selected = library if library is not None else os.environ.get("GR_OS_LIBRARY")
        if not selected:
            raise ValueError("Pass a native library path or set GR_OS_LIBRARY")
        self.path = Path(selected).expanduser().resolve(strict=True)
        self._lib = ctypes.CDLL(str(self.path))
        if not hasattr(self._lib, "gr_blob_read"):
            raise RuntimeError(
                "This SDK requires GR-OS 0.2 binary-reader support (C ABI 1)"
            )
        self._lib.gr_abi_version.argtypes = []
        self._lib.gr_abi_version.restype = ctypes.c_uint32
        self._lib.gr_call_json.argtypes = [
            ctypes.c_char_p,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self._lib.gr_call_json.restype = ctypes.c_int
        self._lib.gr_blob_read.argtypes = [
            ctypes.c_char_p,
            ctypes.c_size_t,
            ctypes.c_uint64,
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self._lib.gr_blob_read.restype = ctypes.c_int
        self._lib.gr_free.argtypes = [ctypes.c_void_p]
        self._lib.gr_free.restype = None
        if self._lib.gr_abi_version() != 1:
            raise RuntimeError("This wrapper requires native C ABI version 1")
        self.capabilities = self.call("sdk.capabilities")
        self._buffer_exporter = None
        if "owned_buffers" in self.capabilities.get("features", []):
            self._lib.gr_buffer_import.argtypes = [
                ctypes.c_void_p,
                ctypes.c_size_t,
                ctypes.c_char_p,
                ctypes.c_size_t,
                ctypes.POINTER(ctypes.c_void_p),
                ctypes.POINTER(ctypes.c_size_t),
            ]
            self._lib.gr_buffer_import.restype = ctypes.c_int
            self._lib.gr_buffer_read.argtypes = self._lib.gr_blob_read.argtypes
            self._lib.gr_buffer_read.restype = ctypes.c_int

    def call(self, method: str, **parameters: Any) -> Any:
        if "abi" in parameters or "op" in parameters:
            raise ValueError("abi and op are reserved transport fields")
        request = json.dumps(
            {"abi": 1, "op": method, **parameters},
            ensure_ascii=False,
            allow_nan=False,
            separators=(",", ":"),
        ).encode("utf-8")
        if len(request) > 16 * 1024 * 1024:
            raise NativeError("request_limit", "C ABI request exceeds 16 MiB")
        response = ctypes.c_void_p()
        size = ctypes.c_size_t()
        status = self._lib.gr_call_json(
            request, len(request), ctypes.byref(response), ctypes.byref(size)
        )
        return self._receive(status, response, size)

    def read_blob(self, handle: str, offset: int, buffer: Any) -> int:
        return self._read_into(self._lib.gr_blob_read, handle, offset, buffer)

    def read_buffer(self, handle: str, offset: int, buffer: Any) -> int:
        return self._read_into(self._lib.gr_buffer_read, handle, offset, buffer)

    def import_buffer(self, data: Any, layout: dict) -> dict:
        view = memoryview(data)
        if view.nbytes > 64 * 1024 * 1024:
            raise NativeError("request_limit", "Buffer import exceeds 64 MiB")
        # Readonly exporter flags do not establish immutable backing ownership.
        copied = view.tobytes(order="C")
        descriptor = json.dumps(
            layout, ensure_ascii=False, allow_nan=False, separators=(",", ":")
        ).encode("utf-8")
        if len(descriptor) > 1024 * 1024:
            raise NativeError("request_limit", "Buffer layout exceeds 1 MiB")
        response, size = ctypes.c_void_p(), ctypes.c_size_t()
        status = self._lib.gr_buffer_import(
            copied,
            len(copied),
            descriptor,
            len(descriptor),
            ctypes.byref(response),
            ctypes.byref(size),
        )
        return self._receive(status, response, size)

    def export_buffer(self, handle: str) -> memoryview:
        if sys.version_info < (3, 11):
            raise RuntimeError(
                "Readonly native export requires CPython 3.11+; use Buffer.to_bytes/readinto"
            )
        if self._buffer_exporter is None:
            candidates = list(self.path.parent.glob("_gr_buffer*.pyd")) + list(
                self.path.parent.glob("_gr_buffer*.so")
            )
            if not candidates:
                raise RuntimeError(
                    "Optional CPython 3.11+ exporter is unavailable; use Buffer.to_bytes/readinto"
                )
            dll_directory = (
                os.add_dll_directory(str(self.path.parent)) if os.name == "nt" else None
            )
            try:
                spec = importlib.util.spec_from_file_location(
                    "_gr_buffer", candidates[0]
                )
                if spec is None or spec.loader is None:
                    raise RuntimeError("Cannot load optional native buffer exporter")
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                if (
                    module.library_identity()
                    != ctypes.cast(self._lib.gr_abi_version, ctypes.c_void_p).value
                ):
                    raise RuntimeError(
                        "Buffer exporter is linked to a different native library"
                    )
                self._buffer_exporter = module
            finally:
                if dll_directory is not None:
                    dll_directory.close()
        return self._buffer_exporter.export(handle)

    def _read_into(self, function, handle: str, offset: int, buffer: Any) -> int:
        if type(offset) is not int or offset < 0 or offset > 2**64 - 1:
            raise ValueError("offset must be a uint64 integer")
        view = memoryview(buffer)
        if view.readonly or not view.c_contiguous:
            raise TypeError("readinto needs a contiguous writable buffer")
        view = view.cast("B")
        output = (ctypes.c_uint8 * view.nbytes).from_buffer(view)
        response = ctypes.c_void_p()
        size = ctypes.c_size_t()
        token = handle.encode("utf-8")
        status = function(
            token,
            len(token),
            offset,
            output,
            view.nbytes,
            ctypes.byref(response),
            ctypes.byref(size),
        )
        return self._receive(status, response, size)["bytes_read"]

    def _receive(
        self, status: int, response: ctypes.c_void_p, size: ctypes.c_size_t
    ) -> Any:
        try:
            if not response.value:
                raise RuntimeError(
                    f"Native transport failed without a response (status {status})"
                )
            envelope = json.loads(
                ctypes.string_at(response, size.value).decode("utf-8")
            )
        finally:
            if response.value:
                self._lib.gr_free(response)
        if envelope.get("abi") != 1 or type(envelope.get("ok")) is not bool:
            raise RuntimeError("Malformed native response envelope")
        if not envelope["ok"]:
            error = envelope["error"]
            raise NativeError(error["code"], error["message"], error.get("details"))
        if status != 0:
            raise RuntimeError(
                f"Native status {status} contradicts successful response"
            )
        return envelope["result"]


_default: Native | None = None
_default_lock = Lock()


def _native(native: Native | None) -> Native:
    global _default
    if native is not None:
        return native
    with _default_lock:
        if _default is None:
            _default = Native()
        return _default


def _release(native: Native, operation: str, handle: str) -> None:
    try:
        native.call(operation, handle=handle)
    except Exception:
        # Finalizers cannot report errors reliably; explicit close does.
        pass


class _Resource:
    def _own(self, native: Native, operation: str, handle: str) -> None:
        self.native = native
        self._handle: str | None = handle
        self._resource_lock = Lock()
        self._close_operation = operation
        self._finalizer = weakref.finalize(self, _release, native, operation, handle)

    def _resource_handle(self) -> str:
        with self._resource_lock:
            if self._handle is None:
                raise NativeError("invalid_handle", "Resource is closed")
            return self._handle

    def _call(self, operation: str, /, **parameters: Any) -> Any:
        return self.native.call(operation, handle=self._resource_handle(), **parameters)

    @property
    def closed(self) -> bool:
        with self._resource_lock:
            return self._handle is None

    def close(self) -> None:
        with self._resource_lock:
            handle = self._handle
            if handle is None:
                return
            self._handle = None
            self._finalizer.detach()
        self.native.call(self._close_operation, handle=handle)

    def __enter__(self):
        self._resource_handle()
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()
