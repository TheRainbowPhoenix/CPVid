#ifndef CPQOI_H
#define CPQOI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decodes CPQOI data and draws it directly to the screen using gint's drect.
 *
 * @param data Compressed CPQOI data.
 * @param offset_x X offset on screen.
 * @param offset_y Y offset on screen.
 */
void cpqoi_decode_and_draw(const uint8_t *data, int offset_x, int offset_y);

/**
 * @brief Encodes raw RGB565 data into CPQOI format.
 *
 * @param src Input raw RGB565 pixel data.
 * @param width Image width.
 * @param height Image height.
 * @param dst Output buffer for CPQOI data.
 * @param dst_len Maximum size of output buffer.
 * @return size_t Actual size of CPQOI data, or 0 on failure.
 */
size_t cpqoi_encode(const uint16_t *src, uint16_t width, uint16_t height, uint8_t *dst, size_t dst_len);

/**
 * @brief Decodes CPQOI data into a raw RGB565 buffer.
 *
 * @param data Compressed CPQOI data.
 * @param dst Output buffer for raw RGB565 pixels.
 * @param dst_len Maximum size of output buffer in bytes.
 * @param width Pointer to store image width.
 * @param height Pointer to store image height.
 * @return size_t Actual size of decompressed data in bytes, or 0 on failure.
 */
size_t cpqoi_decode(const uint8_t *data, uint16_t *dst, size_t dst_len, uint16_t *width, uint16_t *height);

#ifdef __cplusplus
}
#endif

#endif /* CPQOI_H */
