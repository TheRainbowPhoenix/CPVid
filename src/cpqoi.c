#include "cpqoi.h"
#include <gint/display.h>
#include <string.h>
#include <stdlib.h>

#define CPQOI_MAGIC "CPQO"

static inline uint8_t cpqoi_hash(uint16_t c) {
    return (uint8_t)((c ^ (c >> 5) ^ (c >> 11)) & 63);
}

void cpqoi_decode_to_buffer(const uint8_t *data, uint16_t *dst, int stride, int off_x, int off_y) {
    if (memcmp(data, CPQOI_MAGIC, 4) != 0) return;

    uint16_t width, height;
    width = data[4] | (data[5] << 8);
    height = data[6] | (data[7] << 8);

    uint16_t index[64] = {0};
    uint16_t prev_color = 0;
    size_t p = 8;
    int x = 0, y = 0;

    while (y < height) {
        uint8_t b1 = data[p++];

        if (b1 < 0x40) {
            // INDEX
            prev_color = index[b1];
            dst[(off_y + y) * stride + (off_x + x)] = prev_color;
            x += 1;
        } else if (b1 < 0x80) {
            // SKIP
            int skip = (b1 & 0x3F) + 1;
            x += skip;
        } else if (b1 < 0xC0) {
            // RUN
            int run = (b1 & 0x3F) + 1;
            while (run > 0) {
                int to_draw = (x + run > width) ? (width - x) : run;
                uint16_t *line = &dst[(off_y + y) * stride + (off_x + x)];
                for (int i = 0; i < to_draw; i++) line[i] = prev_color;
                x += to_draw;
                run -= to_draw;
                if (x >= width) {
                    y++;
                    x = 0;
                    if (y >= height) break;
                }
            }
            continue;
        } else if (b1 == 0xFF) {
            // RAW
            uint16_t color = data[p] | (data[p+1] << 8);
            p += 2;
            prev_color = color;
            index[cpqoi_hash(color)] = color;
            dst[(off_y + y) * stride + (off_x + x)] = prev_color;
            x += 1;
        } else if (b1 == 0xFE) {
            // LONG SKIP
            int skip = (data[p] | (data[p+1] << 8)) + 1;
            p += 2;
            x += skip;
        } else if (b1 == 0xFD) {
            // LONG RUN
            int run = (data[p] | (data[p+1] << 8)) + 1;
            p += 2;
            while (run > 0) {
                int to_draw = (x + run > width) ? (width - x) : run;
                uint16_t *line = &dst[(off_y + y) * stride + (off_x + x)];
                for (int i = 0; i < to_draw; i++) line[i] = prev_color;
                x += to_draw;
                run -= to_draw;
                if (x >= width) {
                    y++;
                    x = 0;
                    if (y >= height) break;
                }
            }
            continue;
        }

        while (x >= width) {
            y++;
            x -= width;
        }
    }
}

size_t cpqoi_encode(const uint16_t *src, uint16_t width, uint16_t height, uint8_t *dst, size_t dst_len) {
    if (dst_len < 8) return 0;
    memcpy(dst, CPQOI_MAGIC, 4);
    dst[4] = width & 0xFF; dst[5] = (width >> 8) & 0xFF;
    dst[6] = height & 0xFF; dst[7] = (height >> 8) & 0xFF;

    uint16_t index[64] = {0};
    uint16_t prev_color = 0;
    size_t p = 8;
    int total_pixels = width * height;
    int x = 0;

    while (x < total_pixels) {
        uint16_t color = src[x];
        if (color == prev_color) {
            int run = 0;
            while (x + run < total_pixels && src[x + run] == color && run < 65536) run++;
            if (run <= 64) {
                if (p + 1 > dst_len) return 0;
                dst[p++] = 0x80 | (run - 1);
            } else {
                if (p + 3 > dst_len) return 0;
                dst[p++] = 0xFD;
                dst[p++] = (run - 1) & 0xFF;
                dst[p++] = ((run - 1) >> 8) & 0xFF;
            }
            x += run;
        } else {
            uint8_t h = cpqoi_hash(color);
            if (index[h] == color) {
                if (p + 1 > dst_len) return 0;
                dst[p++] = h;
            } else {
                if (p + 3 > dst_len) return 0;
                dst[p++] = 0xFF;
                dst[p++] = color & 0xFF;
                dst[p++] = (color >> 8) & 0xFF;
                index[h] = color;
            }
            prev_color = color;
            x++;
        }
    }
    return p;
}

size_t cpqoi_decode(const uint8_t *data, uint16_t *dst, size_t dst_len, uint16_t *width_out, uint16_t *height_out) {
    if (memcmp(data, CPQOI_MAGIC, 4) != 0) return 0;
    uint16_t width = data[4] | (data[5] << 8);
    uint16_t height = data[6] | (data[7] << 8);
    if (width_out) *width_out = width;
    if (height_out) *height_out = height;

    uint16_t index[64] = {0};
    uint16_t prev_color = 0;
    size_t p = 8;
    int x = 0;
    int total_pixels = width * height;

    while (x < total_pixels) {
        uint8_t b1 = data[p++];
        if (b1 < 0x40) {
            prev_color = index[b1];
            if (x < total_pixels) dst[x++] = prev_color; else return 0;
        } else if (b1 < 0x80) {
            int skip = (b1 & 0x3F) + 1;
            for (int i = 0; i < skip; i++) if (x < total_pixels) dst[x++] = 0;
        } else if (b1 < 0xC0) {
            int run = (b1 & 0x3F) + 1;
            for (int i = 0; i < run; i++) if (x < total_pixels) dst[x++] = prev_color; else return 0;
        } else if (b1 == 0xFF) {
            uint16_t color = data[p] | (data[p+1] << 8);
            p += 2;
            prev_color = color;
            index[cpqoi_hash(color)] = color;
            if (x < total_pixels) dst[x++] = prev_color; else return 0;
        } else if (b1 == 0xFE) {
            int skip = (data[p] | (data[p+1] << 8)) + 1;
            p += 2;
            for (int i = 0; i < skip; i++) if (x < total_pixels) dst[x++] = 0;
        } else if (b1 == 0xFD) {
            int run = (data[p] | (data[p+1] << 8)) + 1;
            p += 2;
            for (int i = 0; i < run; i++) if (x < total_pixels) dst[x++] = prev_color; else return 0;
        }
    }
    return x * 2;
}
