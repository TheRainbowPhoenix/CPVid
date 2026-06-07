#include "lzw.h"
#include <stdlib.h>
#include <string.h>

#define DICT_SIZE 4096
#define MAX_CODE 4095

typedef struct {
    int prefix;
    uint8_t character;
} dict_entry_t;

size_t lzw_encode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len) {
    if (src_len == 0) return 0;

    uint16_t *out = (uint16_t *)dst;
    size_t out_count = 0;
    size_t max_out = dst_len / 2;

    // Simplified hash table for dictionary
    // Key is (prefix << 8) | character
    int16_t head[DICT_SIZE];
    int16_t next[DICT_SIZE];
    dict_entry_t dict[DICT_SIZE];
    memset(head, -1, sizeof(head));

    for (int i = 0; i < 256; i++) {
        dict[i].prefix = -1;
        dict[i].character = i;
        head[i] = i;
        next[i] = -1;
    }

    int next_code = 256;
    int w = src[0];

    for (size_t i = 1; i < src_len; i++) {
        uint8_t c = src[i];

        // Search for wc in dict
        int found_code = -1;
        int h = (w * 31 + c) % DICT_SIZE;
        for (int code = head[h]; code != -1; code = next[code]) {
            if (dict[code].prefix == w && dict[code].character == c) {
                found_code = code;
                break;
            }
        }

        if (found_code != -1) {
            w = found_code;
        } else {
            if (out_count < max_out) {
                out[out_count++] = (uint16_t)w;
            } else {
                return 0; // Buffer too small
            }

            if (next_code < DICT_SIZE) {
                dict[next_code].prefix = w;
                dict[next_code].character = c;
                int h_new = (w * 31 + c) % DICT_SIZE;
                next[next_code] = head[h_new];
                head[h_new] = next_code;
                next_code++;
            }
            w = c;
        }
    }

    if (out_count < max_out) {
        out[out_count++] = (uint16_t)w;
    } else {
        return 0;
    }

    return out_count * 2;
}

size_t lzw_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len) {
    if (src_len == 0) return 0;

    const uint16_t *in = (const uint16_t *)src;
    size_t in_count = src_len / 2;
    size_t out_pos = 0;

    dict_entry_t dict[DICT_SIZE];
    for (int i = 0; i < 256; i++) {
        dict[i].prefix = -1;
        dict[i].character = i;
    }

    int next_code = 256;
    int k = in[0];
    if (k >= 256) return 0; // Invalid first code

    uint8_t w = (uint8_t)k;
    if (out_pos < dst_len) dst[out_pos++] = w; else return 0;

    uint8_t stack[DICT_SIZE];

    for (size_t i = 1; i < in_count; i++) {
        k = in[i];
        int entry_code = k;
        int stack_ptr = 0;

        if (k < next_code) {
            // Code in table
        } else if (k == next_code) {
            // Special case: k == next_code
            stack[stack_ptr++] = w;
            entry_code = in[i-1];
        } else {
            return 0; // Bad code
        }

        while (entry_code >= 256) {
            stack[stack_ptr++] = dict[entry_code].character;
            entry_code = dict[entry_code].prefix;
        }
        uint8_t first = (uint8_t)entry_code;
        stack[stack_ptr++] = first;

        // Output stack in reverse
        while (stack_ptr > 0) {
            if (out_pos < dst_len) {
                dst[out_pos++] = stack[--stack_ptr];
            } else {
                return 0;
            }
        }

        if (next_code < DICT_SIZE) {
            dict[next_code].prefix = in[i-1];
            dict[next_code].character = first;
            next_code++;
        }
        w = first;
    }

    return out_pos;
}
