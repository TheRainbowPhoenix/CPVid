#include "perf.hpp"
#include <gint/display.h>
#include <cstdio>

PerformanceCounters pc_last;

#define RGB24(HEX) \
    (((HEX >> 8) & 0xf800) | \
     ((HEX >> 5) & 0x07e0) | \
     ((HEX >> 3) & 0x001f))

void render_infoview(int frames) {
    drect_border(40, 40, DWIDTH-41, DHEIGHT-41, C_BLACK, 2, C_RGB(10, 10, 10));
    dprint(46, 46, C_WHITE, "Frames: %d", frames);
    dprint(46, 61, C_WHITE, "FPS: %d (%d ms)", pc_last.fps, prof_time(pc_last.frame_total) / 1000);

    static int colors[] = {
        RGB24(0x264653),
        RGB24(0x2a9d8f),
        RGB24(0xe9c46a),
        RGB24(0xf4a261)
    };
    static const char* desc[] = {
        "File read",
        "Decode",
        "Display update",
        "Other"
    };

    uint32_t total_us = prof_time(pc_last.frame_total);
    if (total_us > 0) {
        uint32_t times[4];
        times[0] = prof_time(pc_last.file_read);
        times[1] = prof_time(pc_last.decode);
        times[2] = prof_time(pc_last.display_update);
        uint32_t measured = times[0] + times[1] + times[2];
        times[3] = (measured < total_us) ? (total_us - measured) : 0;

        int fields[4], w = 120;
        for (int i = 0; i < 4; i++)
            fields[i] = (int)((uint64_t)times[i] * w / total_us);

        int bx = 50, by = 78, bh = 8;
        drect(bx, by, bx + w - 1, by + bh - 1, C_RGB(15, 15, 15));

        int cur_x = bx;
        for (int i = 0; i < 4; i++) {
            if (fields[i] > 0) {
                drect(cur_x, by, cur_x + fields[i] - 1, by + bh - 1, colors[i]);
                cur_x += fields[i];
            }
        }

        dprint(50 + w + 4, by, C_WHITE, "= %d.%d ms", total_us / 1000, (total_us / 100) % 10);

        int lx = 50, ly = by + bh + 15;
        for (int i = 0; i < 4; i++) {
            drect(lx, ly + 1, lx + 6, ly + 7, colors[i]);
            dtext(lx + 12, ly, C_WHITE, desc[i]);
            ly += 11;
        }
    }
}
