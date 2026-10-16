#pragma once
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#if defined(GR_MODULE_EXPORTS)
#define GR_ANALYSIS_API __declspec(dllexport)
#else
#define GR_ANALYSIS_API __declspec(dllimport)
#endif
#else
#define GR_ANALYSIS_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Trusted synchronous API. Metadata <=1 MiB; copied raw source <=64 MiB.
 * Returns 0 success, 1 structured error, 2 transport/allocation failure.
 * All response allocations, including errors, must use this DLL's free.
 * Handles belong only to this DLL; close cannot invalidate an acquired call.
 * Unloading the DLL while handles/calls remain is unsupported. */
GR_ANALYSIS_API uint32_t gr_analysis_abi_version(void);
GR_ANALYSIS_API int gr_analysis_call_v1(const char *request, size_t request_size, const uint8_t *payload,
                                        size_t payload_size, char **response, size_t *response_size);
GR_ANALYSIS_API void gr_analysis_free(void *allocation);
#ifdef __cplusplus
}
#endif
