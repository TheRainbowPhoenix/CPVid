#include "player.hpp"
#include "gmpak.hpp"
#include "cpqoi.h"
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/clock.h>
#include <ctime>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>

// Custom ticks_ms using clock()
static uint32_t ticks_ms() {
    return (uint32_t)((uint64_t)clock() * 1000 / CLOCKS_PER_SEC);
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

            dsubimage(dx, dy, img, sx, sy, tw, th, DIMAGE_NONE);
        }
        // Update per row as in the Python version
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

void play_video(const std::string& pak_file) {
    draw_loading_logo();

    GMPak pak(pak_file);
    if (!pak.is_open()) {
        dclear(C_BLACK);
        dtext(10, 30, C_RED, "Failed to load VIDEO.");
        dupdate();
        sleep_ms(2000);
        return;
    }

    std::vector<uint8_t> meta_bytes = pak.get_entry("META");
    std::string meta_data((char*)meta_bytes.data(), meta_bytes.size());
    int fps = 15, total_frames = 0, w = 320, h = 320;

    std::stringstream ss(meta_data);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("fps=") == 0) fps = std::stoi(line.substr(4));
        if (line.find("frames=") == 0) total_frames = std::stoi(line.substr(7));
        if (line.find("width=") == 0) w = std::stoi(line.substr(6));
        if (line.find("height=") == 0) h = std::stoi(line.substr(7));
    }

    int frame_duration_ms = 1000 / fps;
    int offset_x = (DWIDTH - w) / 2; if (offset_x < 0) offset_x = 0;
    int offset_y = (DHEIGHT - h) / 2; if (offset_y < 0) offset_y = 0;

    dclear(C_BLACK);
    drect(offset_x, offset_y, offset_x + w - 1, offset_y + h - 1, C_WHITE);
    dupdate();

    bool playing = true;
    int frame_idx = 0;
    int show_icon_timer = 2;

    while (true) {
        uint32_t start_t = ticks_ms();
        cleareventflips();
        clearevents();

        char frame_name[16];
        sprintf(frame_name, "FRM_%04d", frame_idx);

        if (pak.toc.count(frame_name)) {
            std::vector<uint8_t> binary_data = pak.get_entry(frame_name);
            uint8_t type = pak.toc[frame_name].type;

            if (type == 1) {
                cpqoi_decode_and_draw(binary_data.data(), offset_x, offset_y);
            } else if (type == 4) {
                if (binary_data.size() >= 14) {
                    uint8_t prof = binary_data[0];
                    uint16_t fw, fh, stride;
                    uint8_t cc;
                    uint16_t pal_len;
                    uint32_t data_len;
                    memcpy(&fw, &binary_data[1], 2);
                    memcpy(&fh, &binary_data[3], 2);
                    memcpy(&stride, &binary_data[5], 2);
                    cc = binary_data[7];
                    memcpy(&pal_len, &binary_data[8], 2);
                    memcpy(&data_len, &binary_data[10], 4);

                    const uint8_t *palette_ptr = (pal_len > 0) ? &binary_data[14] : nullptr;
                    const uint8_t *data_ptr = &binary_data[14 + pal_len];

                    image_t img;
                    img.format = prof; // Prof here should map to format
                    img.flags = IMAGE_FLAGS_DATA_RO | IMAGE_FLAGS_PALETTE_RO;
                    img.color_count = cc;
                    img.width = fw;
                    img.height = fh;
                    img.stride = stride;
                    img.data = (void*)data_ptr;
                    img.palette = (uint16_t*)palette_ptr;

                    draw_tiled_gint_image(&img, offset_x, offset_y, fw, fh, 32);
                }
            }
        }

        if (!playing || show_icon_timer > 0) {
            if (!playing) {
                drect(285, 15, 293, 35, C_WHITE);
                drect(299, 15, 307, 35, C_WHITE);
                dupdate();
            } else {
                int px[] = {285, 285, 307};
                int py[] = {15, 35, 25};
                dpoly(px, py, 3, C_WHITE, C_WHITE);
                show_icon_timer--;
            }
        }

        if (pak.toc.count(frame_name) && pak.toc[frame_name].type == 1 || show_icon_timer > 0) {
            dupdate();
        }

        if (keydown(KEY_DEL)) break;
        if (keydown(KEY_EXE)) {
            playing = !playing;
            if (playing) show_icon_timer = 4;
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
            if (frame_idx >= total_frames) frame_idx = 0;
        }

        if (playing || frame_advanced) {
            uint32_t elapsed = ticks_ms() - start_t;
            int sleep_ms_val = frame_duration_ms - (int)elapsed;
            if (sleep_ms_val > 0) sleep_ms(sleep_ms_val);
        } else {
            sleep_ms(50);
        }
    }
}
