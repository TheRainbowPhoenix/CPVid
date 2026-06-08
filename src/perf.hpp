#ifndef PERF_HPP
#define PERF_HPP

#include <gint/prof.h>
#include <stdint.h>

struct PerformanceCounters {
    prof_t file_read;
    prof_t decode;
    prof_t display_update;
    prof_t frame_total;
    int fps;
};

extern PerformanceCounters pc_last;

void render_infoview(int frames);

#endif // PERF_HPP
