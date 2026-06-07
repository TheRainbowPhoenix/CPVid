#ifndef LZW_H
#define LZW_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compresses data using LZW algorithm.
 *
 * @param src Input data buffer.
 * @param src_len Length of input data.
 * @param dst Output buffer for compressed data.
 * @param dst_len Maximum size of output buffer.
 * @return size_t Actual size of compressed data, or 0 on failure.
 */
size_t lzw_encode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len);

/**
 * @brief Decompresses data using LZW algorithm.
 *
 * @param src Compressed input data buffer.
 * @param src_len Length of compressed input data.
 * @param dst Output buffer for decompressed data.
 * @param dst_len Maximum size of output buffer.
 * @return size_t Actual size of decompressed data, or 0 on failure.
 */
size_t lzw_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len);

#ifdef __cplusplus
}
#endif

#endif /* LZW_H */
