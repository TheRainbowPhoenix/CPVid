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

    size_t out_pos = 0;

    // Simplified hash table for dictionary
    // Key is (prefix << 8) | character
    int16_t *head = (int16_t*)malloc(sizeof(int16_t) * DICT_SIZE);
    int16_t *next = (int16_t*)malloc(sizeof(int16_t) * DICT_SIZE);
    dict_entry_t *dict = (dict_entry_t*)malloc(sizeof(dict_entry_t) * DICT_SIZE);

    if (!head || !next || !dict) {
        if (head) free(head);
        if (next) free(next);
        if (dict) free(dict);
        return 0;
    }

    memset(head, -1, sizeof(int16_t) * DICT_SIZE);

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
            if (out_pos + 2 <= dst_len) {
                // Little Endian output
                dst[out_pos++] = w & 0xFF;
                dst[out_pos++] = (w >> 8) & 0xFF;
            } else {
                free(head); free(next); free(dict);
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

    if (out_pos + 2 <= dst_len) {
        dst[out_pos++] = w & 0xFF;
        dst[out_pos++] = (w >> 8) & 0xFF;
    } else {
        out_pos = 0;
    }

    free(head);
    free(next);
    free(dict);
    return out_pos;
}

size_t lzw_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len) {
    if (src_len == 0) return 0;

    size_t in_pos = 0;
    size_t out_pos = 0;

    dict_entry_t *dict = (dict_entry_t*)malloc(sizeof(dict_entry_t) * DICT_SIZE);
    uint8_t *stack = (uint8_t*)malloc(DICT_SIZE);

    if (!dict || !stack) {
        if (dict) free(dict);
        if (stack) free(stack);
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        dict[i].prefix = -1;
        dict[i].character = i;
    }

    int next_code = 256;
    if (in_pos + 2 > src_len) return 0;
    int k = src[in_pos] | (src[in_pos + 1] << 8);
    in_pos += 2;

    if (k >= 256) return 0; // Invalid first code

    uint8_t w = (uint8_t)k;
    if (out_pos < dst_len) dst[out_pos++] = w; else { free(dict); free(stack); return 0; }

    int prev_k = k;

    while (in_pos + 2 <= src_len) {
        k = src[in_pos] | (src[in_pos + 1] << 8);
        in_pos += 2;

        int entry_code = k;
        int stack_ptr = 0;

        if (k < next_code) {
            // Code in table
        } else if (k == next_code) {
            // Special case: k == next_code
            stack[stack_ptr++] = w;
            entry_code = prev_k;
        } else {
            free(dict);
            free(stack);
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
                free(dict);
                free(stack);
                return 0;
            }
        }

        if (next_code < DICT_SIZE) {
            dict[next_code].prefix = prev_k;
            dict[next_code].character = first;
            next_code++;
        }
        w = first;
        prev_k = k;
    }

    free(dict);
    free(stack);
    return out_pos;
}
