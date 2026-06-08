#include "lzw.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int prefix;
    uint8_t character;
} lzw_node_t;

size_t lzw_encode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len) {
    if (src_len == 0 || dst_len < 2) return 0;

    int *dict_prefix = (int*)malloc(65536 * sizeof(int));
    uint8_t *dict_char = (uint8_t*)malloc(65536 * sizeof(uint8_t));
    if (!dict_prefix || !dict_char) {
        free(dict_prefix); free(dict_char);
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        dict_prefix[i] = -1;
        dict_char[i] = (uint8_t)i;
    }
    int next_code = 256;
    int current_prefix = -1;
    size_t p = 0;

    for (size_t i = 0; i < src_len; i++) {
        uint8_t c = src[i];
        int found = -1;
        if (current_prefix == -1) {
            found = c;
        } else {
            for (int j = 256; j < next_code; j++) {
                if (dict_prefix[j] == current_prefix && dict_char[j] == c) {
                    found = j;
                    break;
                }
            }
        }

        if (found != -1) {
            current_prefix = found;
        } else {
            if (p + 2 > dst_len) {
                free(dict_prefix); free(dict_char);
                return 0;
            }
            dst[p++] = (uint8_t)(current_prefix & 0xFF);
            dst[p++] = (uint8_t)((current_prefix >> 8) & 0xFF);

            if (next_code < 65536) {
                dict_prefix[next_code] = current_prefix;
                dict_char[next_code] = c;
                next_code++;
            }
            current_prefix = c;
        }
    }

    if (current_prefix != -1) {
        if (p + 2 > dst_len) {
            free(dict_prefix); free(dict_char);
            return 0;
        }
        dst[p++] = (uint8_t)(current_prefix & 0xFF);
        dst[p++] = (uint8_t)((current_prefix >> 8) & 0xFF);
    }

    free(dict_prefix); free(dict_char);
    return p;
}

size_t lzw_decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_len) {
    if (src_len < 2) return 0;

    lzw_node_t *dict = (lzw_node_t*)malloc(65536 * sizeof(lzw_node_t));
    uint8_t *stack = (uint8_t*)malloc(65536);
    if (!dict || !stack) {
        free(dict); free(stack);
        return 0;
    }

    for (int i = 0; i < 256; i++) {
        dict[i].prefix = -1;
        dict[i].character = (uint8_t)i;
    }
    int next_code = 256;

    size_t p_src = 0;
    size_t p_dst = 0;

    uint16_t code = src[p_src] | (src[p_src+1] << 8);
    p_src += 2;
    uint16_t old_code = code;

    if (code >= 256) {
        free(dict); free(stack);
        return 0;
    }
    dst[p_dst++] = (uint8_t)code;
    uint8_t k = (uint8_t)code;

    while (p_src + 1 < src_len) {
        code = src[p_src] | (src[p_src+1] << 8);
        p_src += 2;

        int temp_code = code;
        int stack_ptr = 0;

        if (temp_code < next_code) {
            while (temp_code != -1) {
                stack[stack_ptr++] = dict[temp_code].character;
                temp_code = dict[temp_code].prefix;
            }
        } else {
            stack[stack_ptr++] = k;
            temp_code = old_code;
            while (temp_code != -1) {
                stack[stack_ptr++] = dict[temp_code].character;
                temp_code = dict[temp_code].prefix;
            }
        }

        k = stack[stack_ptr - 1];
        while (stack_ptr > 0) {
            if (p_dst < dst_len) dst[p_dst++] = stack[--stack_ptr];
            else {
                free(dict); free(stack);
                return 0;
            }
        }

        if (next_code < 65536) {
            dict[next_code].prefix = old_code;
            dict[next_code].character = k;
            next_code++;
        }
        old_code = code;
    }

    free(dict); free(stack);
    return p_dst;
}
