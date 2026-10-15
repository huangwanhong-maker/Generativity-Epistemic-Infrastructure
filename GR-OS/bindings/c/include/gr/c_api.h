#ifndef GR_C_API_H
#define GR_C_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(GR_NATIVE_EXPORTS)
#define GR_API __declspec(dllexport)
#else
#define GR_API __declspec(dllimport)
#endif
#else
#define GR_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Version 1: a trusted, local library boundary, not an authorization service.
 * gr_call_json accepts a UTF-8 JSON request with {"abi":1,"op":"..."}.
 * The result is an allocated UTF-8 JSON envelope:
 *   {"ok":true,"result":...} or {"ok":false,"error":{...}}.
 * Return value: 0 success, 1 reported operation failure, 2 invalid pointers or
 * allocation failure. A non-null response must always be freed with gr_free.
 * Input length excludes a terminating NUL. Embedded NUL bytes are rejected.
 * Requests are bounded to 16 MiB. No C++ exception crosses this boundary.
 */
GR_API uint32_t gr_abi_version(void);
GR_API int gr_call_json(const char *request, size_t request_size, char **response, size_t *response_size);
GR_API void gr_free(void *allocation);
/* Read a verified pinned blob into caller-owned writable storage. Obtain the
 * opaque reader handle through store.blob.open; release with blob.close.
 * The JSON response contains bytes_read or the same structured error envelope.
 * Capacity is bounded by the reader's configured chunk size. A null destination
 * is valid only for zero capacity. Input/output allocations remain caller-owned.
 * Concurrent reads serialize per reader; close preserves in-flight calls.
 */
GR_API int gr_blob_read(const char *handle, size_t handle_size, uint64_t offset, uint8_t *destination,
                        size_t capacity, char **response, size_t *response_size);

/* Additive ABI 1 buffer support. Import always copies caller storage. Layout is
 * bounded UTF-8 JSON {element:{format_id,item_size,alignment,byte_order},shape,
 * strides,byte_offset}; omitted layout fields describe contiguous octets.
 * Native handles are released through buffer.close. Read copies contiguous
 * logical bytes; strided views first require buffer.copy_contiguous.
 */
GR_API int gr_buffer_import(const uint8_t *data, size_t data_size, const char *layout, size_t layout_size,
                            char **response, size_t *response_size);
GR_API int gr_buffer_read(const char *handle, size_t handle_size, uint64_t offset, uint8_t *destination,
                          size_t capacity, char **response, size_t *response_size);
typedef struct gr_buffer_lease gr_buffer_lease;
typedef struct gr_buffer_info {
    const uint8_t *data;
    size_t logical_bytes;
    size_t item_size;
    size_t ndim;
    const uint64_t *shape;
    const int64_t *strides;
    const char *format_id;
    uint32_t byte_order;   /* 0 not applicable, 1 little, 2 big */
    uint32_t known_format; /* 1 exact unparameterized int64; 0 opaque */
} gr_buffer_info;
/* Pin returns an immutable borrowed pointer/layout owned by the separate lease.
 * Closing the source handle does not invalidate it. Caller must keep the native
 * library loaded and release the lease only after all pointer users finish.
 * This trusted C boundary cannot prevent casts that violate const correctness.
 */
GR_API int gr_buffer_pin(const char *handle, size_t handle_size, gr_buffer_lease **lease,
                         gr_buffer_info *info, char **response, size_t *response_size);
GR_API void gr_buffer_unpin(gr_buffer_lease *lease);

#ifdef __cplusplus
}
#endif
#endif
