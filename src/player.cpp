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

static void draw_tiled_gint_image(const image_t *img, int x0, int y0, int img_w, int img_h, int tile_size = 32) {
    int cols = img_w / tile_size + (img_w % tile_size != 0 ? 1 : 0);
    int rows = img_h / tile_size + (img_h % tile_size != 0 ? 1 : 0);

    for (int ty = 0; ty < rows; ty++) {
        int dy = y0 + ty * tile_size;
        int sy = ty * tile_size;
        int th = (sy + tile_size <= img_h) ? tile_size : img_h - sy;

        for (int tx = 0; tx < cols; tx++) {
            int dx = x0 + tx * tile_size;
            int sx = tx * tile_size;
            int tw = (sx + tile_size <= img_w) ? tile_size : img_w - sx;

            dsubimage(dx, dy, (image_t*)img, sx, sy, tw, th, DIMAGE_NONE);
        }
        // Restore row-by-row update to match Python's wipe effect
        dupdate();
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
    sleep_ms(1000);
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
        // If not cached, meta_ptr was malloced in get_entry.
        // Currently get_entry always mallocs for non-cached or returns cached.
        // Let's check if it's cached.
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

    int frame_duration_ms = 1000 / fps;
    int offset_x = (DWIDTH - w) / 2; if (offset_x < 0) offset_x = 0;
    int offset_y = (DHEIGHT - h) / 2; if (offset_y < 0) offset_y = 0;

    dclear(C_BLACK);
    drect(offset_x, offset_y, offset_x + w - 1, offset_y + h - 1, C_WHITE);
    dupdate();

    bool playing = true;
    bool infoview = false;
    int frame_idx = 0;
    int show_icon_timer = 2;
    int total_frames_played = 0;

    while (true) {
        pc_last.frame_total = prof_make();
        prof_enter(pc_last.frame_total);

        uint32_t start_t = ticks_ms();
        cleareventflips();
        clearevents();

        if (playing) {
            pak.preload_entries("FRM_", frame_idx + 1, frame_idx + 5);
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
                cpqoi_decode_and_draw(binary_data, offset_x, offset_y);
            } else if (type == 4) {
                if (bin_size >= 14) {
                    uint8_t prof = binary_data[0];
                    uint16_t fw, fh, stride;
                    uint8_t cc;
                    uint16_t pal_len;

                    fw = binary_data[1] | (binary_data[2] << 8);
                    fh = binary_data[3] | (binary_data[4] << 8);
                    stride = binary_data[5] | (binary_data[6] << 8);
                    cc = binary_data[7];
                    pal_len = binary_data[8] | (binary_data[9] << 8);

                    const uint8_t *palette_ptr = (pal_len > 0) ? &binary_data[14] : nullptr;
                    const uint8_t *data_ptr = &binary_data[14 + pal_len];

                    image_t img;
                    img.format = prof;
                    img.flags = (uint8_t)(IMAGE_FLAGS_DATA_RO | IMAGE_FLAGS_PALETTE_RO);
                    img.color_count = cc;
                    img.width = fw;
                    img.height = fh;
                    img.stride = stride;
                    img.data = (void*)data_ptr;
                    img.palette = (uint16_t*)palette_ptr;

                    draw_tiled_gint_image(&img, offset_x, offset_y, fw, fh, 32);
                }
            }
            prof_leave(pc_last.decode);

            bool is_cached = false;
            for(uint32_t i=0; i<pak.entry_count; i++) if(pak.entries[i].cached_data == bin_ptr) is_cached = true;
            if(!is_cached) free(bin_ptr);
        }

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

        pc_last.display_update = prof_make();
        prof_enter(pc_last.display_update);
        if (infoview) {
            render_infoview(total_frames_played);
        }

        if ((bin_ptr && type == 1) || (!playing || show_icon_timer > 0 || infoview)) {
            dupdate();
        }
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
            if (frame_idx >= total_frames) frame_idx = 0;
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
    prof_quit();
}
