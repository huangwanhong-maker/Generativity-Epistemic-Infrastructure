# GR-OS Python SDK

This package contains the standard-library Python facade. Install the GR-OS
native SDK separately and pass `Native(path)` or set `GR_OS_LIBRARY` to its
shared library. This wheel does not contain a native binary and does not download
or install one. C ABI version 1 with the GR-OS 0.3 runtime API is required.

Use `Storage`, `BlobReader`, `Runtime`, `Plan`, `Job`, and `Transaction` with
explicit close or context managers. Transport calls are synchronous; Runtime
submits native asynchronous jobs with polling and asyncio access. Callback
registration remains a trusted C++ interface. See `docs/sdk.md` and
`docs/runtime-sdk.md` in the native SDK workspace for installation, ownership,
publication recovery, and limits.
