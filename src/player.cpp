#include "player.hpp"
#include "gmpak.hpp"
#include "cpqoi.h"
#include "perf.hpp"
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/clock.h>
#include <gint/rtc.h>
#include <gint/gint.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static uint32_t ticks_ms() {
    return (uint32_t)((uint64_t)rtc_ticks() * 1000 / 128);
}

// Draw a gint image to a buffer (usually VRAM)
static void draw_gint_image_to_buffer(const image_t *img, uint16_t *dst, int stride, int x0, int y0) {
    if (img->format == IMAGE_RGB565 || img->format == IMAGE_RGB565A) {
        const uint16_t *src = (const uint16_t *)img->data;
        for (int y = 0; y < img->height; y++) {
            for (int x = 0; x < img->width; x++) {
                dst[(y0 + y) * stride + (x0 + x)] = src[y * img->stride + x];
            }
        }
    } else if (img->format == IMAGE_P8_RGB565 || img->format == IMAGE_P8_RGB565A) {
        const uint8_t *src = (const uint8_t *)img->data;
        const uint16_t *pal = img->palette;
        for (int y = 0; y < img->height; y++) {
            for (int x = 0; x < img->width; x++) {
                dst[(y0 + y) * stride + (x0 + x)] = pal[src[y * img->stride + x]];
            }
        }
    } else if (img->format == IMAGE_P4_RGB565 || img->format == IMAGE_P4_RGB565A) {
        const uint8_t *src = (const uint8_t *)img->data;
        const uint16_t *pal = img->palette;
        for (int y = 0; y < img->height; y++) {
            for (int x = 0; x < img->width; x++) {
                uint8_t byte = src[y * img->stride + (x >> 1)];
                uint8_t idx = (x & 1) ? (byte & 0x0f) : (byte >> 4);
                dst[(y0 + y) * stride + (x0 + x)] = pal[idx];
            }
        }
    }
}

static void draw_mono_to_buffer(uint16_t *dst, int stride, int x0, int y0, uint16_t w, uint16_t h, const uint8_t *data) {
    int bytes_per_line = (w + 7) / 8;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t byte = data[y * bytes_per_line + (x / 8)];
            bool set = (byte & (0x80 >> (x % 8)));
            dst[(y0 + y) * stride + (x0 + x)] = set ? C_BLACK : C_WHITE;
        }
    }
}

static void draw_loading_logo() {
    dclear(C_BLACK);
    uint16_t color_purple = C_RGB(18, 5, 25);
    uint16_t color_dark_purple = C_RGB(10, 2, 15);

    int cx = 160, cy = 264;
    dcircle(cx, cy, 50, C_WHITE, C_NONE);
    dcircle(cx, cy, 48, color_purple, C_NONE);
    dcircle(cx, cy + 4, 40, color_dark_purple, C_NONE);
    dcircle(cx, cy, 35, C_WHITE, C_NONE);

    int poly_x[] = {cx - 10, cx - 10, cx + 15};
    int poly_y[] = {cy - 14, cy + 14, cy};
    dpoly(poly_x, poly_y, 3, color_purple, color_purple);

    dtext(cx - 56, cy + 70, C_WHITE, "Loading VIDEO");
    dupdate();
    sleep_ms(500);
}

void play_video(const char* pak_file) {
    prof_init();
    draw_loading_logo();

    GMPak pak(pak_file);
    if (!pak.is_open()) {
        dclear(C_BLACK);
        dtext(10, 30, C_RED, "Failed to load VIDEO.");
        dupdate();
        sleep_ms(2000);
        prof_quit();
        return;
    }

    uint32_t meta_size;
    void* meta_ptr = pak.get_entry("META", &meta_size, nullptr);
    int fps = 15, total_frames = 0, w = 320, h = 320;

    if (meta_ptr) {
        char* meta_data = (char*)malloc(meta_size + 1);
        if (meta_data) {
            memcpy(meta_data, meta_ptr, meta_size);
            meta_data[meta_size] = '\0';

            char* line = strtok(meta_data, "\n");
            while (line) {
                if (strncmp(line, "fps=", 4) == 0) fps = atoi(line + 4);
                else if (strncmp(line, "frames=", 7) == 0) total_frames = atoi(line + 7);
                else if (strncmp(line, "width=", 6) == 0) w = atoi(line + 6);
                else if (strncmp(line, "height=", 7) == 0) h = atoi(line + 7);
                line = strtok(nullptr, "\n");
            }
            free(meta_data);
        }
        bool is_meta_cached = false;
        for(uint32_t i=0; i<pak.entry_count; i++) if(pak.entries[i].cached_data == meta_ptr) is_meta_cached = true;
        if(!is_meta_cached) free(meta_ptr);
    }

    if (total_frames <= 0) {
        dclear(C_BLACK);
        dtext(10, 30, C_RED, "No frames in VIDEO.");
        dupdate();
        sleep_ms(2000);
        prof_quit();
        return;
    }

    uint16_t *video_buffer = (uint16_t*)malloc(w * h * 2);
    if (!video_buffer) {
        dclear(C_BLACK);
        dtext(10, 30, C_RED, "Out of RAM for buffer.");
        dupdate();
        sleep_ms(2000);
        prof_quit();
        return;
    }
    memset(video_buffer, 0, w * h * 2);

    int frame_duration_ms = 1000 / fps;
    int offset_x = (DWIDTH - w) / 2; if (offset_x < 0) offset_x = 0;
    int offset_y = (DHEIGHT - h) / 2; if (offset_y < 0) offset_y = 0;

    dclear(C_BLACK);
    dupdate();

    bool playing = true;
    bool infoview = false;
    int frame_idx = 0;
    int show_icon_timer = 2;
    int total_frames_played = 0;
    bool first_frame = true;

    while (true) {
        pc_last.frame_total = prof_make();
        prof_enter(pc_last.frame_total);

        uint32_t start_t = ticks_ms();
        cleareventflips();
        clearevents();

        if (playing) {
            // Reduced preloading to save heap
            pak.preload_entries("FRM_", frame_idx + 1, frame_idx + 2);
        }

        char frame_name[16];
        sprintf(frame_name, "FRM_%04d", frame_idx);

        uint32_t bin_size;
        uint8_t type;

        pc_last.file_read = prof_make();
        prof_enter(pc_last.file_read);
        void* bin_ptr = pak.get_entry(frame_name, &bin_size, &type);
        prof_leave(pc_last.file_read);

        if (bin_ptr) {
            uint8_t* binary_data = (uint8_t*)bin_ptr;

            pc_last.decode = prof_make();
            prof_enter(pc_last.decode);
            if (type == 1) {
                cpqoi_decode_to_buffer(binary_data, video_buffer, w, 0, 0);
            } else if (type == 4) {
                // Type 4: Native fxconv gint.image binaries
                if (bin_size >= 14) {
                    image_t img;
                    img.format = binary_data[0];
                    img.width = binary_data[1] | (binary_data[2] << 8);
                    img.height = binary_data[3] | (binary_data[4] << 8);
                    img.stride = binary_data[5] | (binary_data[6] << 8);
                    img.color_count = (int8_t)binary_data[7];
                    uint16_t pal_len = binary_data[8] | (binary_data[9] << 8);
                    img.palette = (uint16_t*)&binary_data[14];
                    img.data = (void*)&binary_data[14 + pal_len];

                    draw_gint_image_to_buffer(&img, video_buffer, w, 0, 0);
                }
            } else if (type == 5) {
                // Type 5: Monochrome fxconv payload
                if (bin_size >= 9) {
                    uint16_t fw = binary_data[1] | (binary_data[2] << 8);
                    uint16_t fh = binary_data[3] | (binary_data[4] << 8);
                    draw_mono_to_buffer(video_buffer, w, 0, 0, fw, fh, &binary_data[9]);
                }
            }
            prof_leave(pc_last.decode);

            bool is_cached = false;
            for(uint32_t i=0; i<pak.entry_count; i++) if(pak.entries[i].cached_data == bin_ptr) is_cached = true;
            if(!is_cached) free(bin_ptr);
        }

        pc_last.display_update = prof_make();
        prof_enter(pc_last.display_update);

        uint16_t *vram = gint_vram;
        for (int line = 0; line < h; line++) {
            memcpy(&vram[(offset_y + line) * DWIDTH + offset_x], &video_buffer[line * w], w * 2);
            // DO NOT dupdate() here! Only after full frame or for wipe effect
            if (first_frame) dupdate();
        }
        first_frame = false;

        if (!playing || show_icon_timer > 0) {
            if (!playing) {
                drect(285, 15, 293, 35, C_WHITE);
                drect(299, 15, 307, 35, C_WHITE);
            } else {
                int px[] = {285, 285, 307};
                int py[] = {15, 35, 25};
                dpoly(px, py, 3, C_WHITE, C_WHITE);
                show_icon_timer--;
            }
        }

        if (infoview) {
            render_infoview(total_frames_played);
        }

        dupdate();
        prof_leave(pc_last.display_update);

        if (keydown(KEY_DEL)) break;
        if (keydown(KEY_EXE)) {
            playing = !playing;
            if (playing) show_icon_timer = 4;
        }
        if (keydown(KEY_KBD)) {
            infoview = !infoview;
            while(keydown(KEY_KBD));
        }
        if (keydown(KEY_MENU)) {
            gint_osmenu();
        }

        bool frame_advanced = false;
        if (keydown(KEY_RIGHT) || keydown(KEY_6)) {
            frame_idx = (frame_idx + 1) % total_frames;
            frame_advanced = true;
            playing = false;
        }
        if (keydown(KEY_LEFT) || keydown(KEY_4)) {
            frame_idx = (frame_idx - 1 + total_frames) % total_frames;
            frame_advanced = true;
            playing = false;
        }

        if (playing && !frame_advanced) {
            frame_idx++;
            total_frames_played++;
            if (frame_idx >= total_frames) {
                frame_idx = 0;
            }
        }

        prof_leave(pc_last.frame_total);
        pc_last.fps = (prof_time(pc_last.frame_total) > 0) ? (1000000 / prof_time(pc_last.frame_total)) : 0;

        if (playing || frame_advanced) {
            uint32_t elapsed = ticks_ms() - start_t;
            int sleep_ms_val = frame_duration_ms - (int)elapsed;
            if (sleep_ms_val > 0) sleep_ms(sleep_ms_val);
        } else {
            sleep_ms(50);
        }
    }

    free(video_buffer);
    prof_quit();
}
