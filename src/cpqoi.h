#ifndef CPQOI_H
#define CPQOI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decodes a CPQOI frame into a 16-bit buffer.
// dst: pointer to the start of the buffer
// stride: width of the buffer in pixels
// off_x, off_y: offset within the buffer to start drawing
void cpqoi_decode_to_buffer(const uint8_t *data, uint16_t *dst, int stride, int off_x, int off_y);

// Original standalone decoder (legacy/convenience)
// void cpqoi_decode_and_draw(const uint8_t *data, int x, int y); // Removed to encourage buffer usage

// Encodes a 16-bit raw image into CPQOI.
size_t cpqoi_encode(const uint16_t *src, uint16_t width, uint16_t height, uint8_t *dst, size_t dst_len);

// Decodes CPQOI into a raw 16-bit buffer.
size_t cpqoi_decode(const uint8_t *data, uint16_t *dst, size_t dst_len, uint16_t *width_out, uint16_t *height_out);

#ifdef __cplusplus
}
#endif

#endif
