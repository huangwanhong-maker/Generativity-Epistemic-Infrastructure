#pragma once
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#if defined(GR_MODULE_EXPORTS)
#define GR_VISUAL_API __declspec(dllexport)
#else
#define GR_VISUAL_API __declspec(dllimport)
#endif
#else
#define GR_VISUAL_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Trusted synchronous copied-snapshot interface. JSON metadata <=1 MiB,
 * raw snapshot <=64 MiB. Returns 0 success, 1 structured error, 2 transport
 * failure. The caller must use this DLL's deallocator for all responses. */
GR_VISUAL_API uint32_t gr_visualization_abi_version(void);
GR_VISUAL_API int gr_visualization_call_v1(const char *request, size_t request_size, const uint8_t *snapshot,
                                           size_t snapshot_size, char **response, size_t *response_size);
GR_VISUAL_API void gr_visualization_free(void *allocation);
#ifdef __cplusplus
}
#endif
