#pragma once
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#if defined(GR_DIAGONAL_EXPORTS)
#define GR_DIAGONAL_API __declspec(dllexport)
#else
#define GR_DIAGONAL_API __declspec(dllimport)
#endif
#else
#define GR_DIAGONAL_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
GR_DIAGONAL_API uint32_t gr_diagonal_abi_version(void);
/* Synchronous package-specific API. Inputs copied before native planning. Output
 * is written only after successful native execution and report allocation.
 * max1,048,576 elements; timeout_ms0 means no deadline. Report always UTF8 JSON,
 * allocated by this DLL, freed only with gr_diagonal_free. Return0 success,
 * 1 structured failure,2 transport/allocation failure. No exceptions cross ABI. */
GR_DIAGONAL_API int gr_diagonal_apply_i64_v1(const int64_t *diagonal, size_t rows, const int64_t *vector,
                                             size_t length, int64_t *output, size_t capacity,
                                             uint64_t timeout_ms, char **report, size_t *report_size);
GR_DIAGONAL_API void gr_diagonal_free(void *);
#ifdef __cplusplus
}
#endif
