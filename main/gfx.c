#include <string.h>

#include "gfx.h"

static uint8_t s_fb[OLED_FB_SIZE];

const uint8_t *gfx_fb(void)
{
    return s_fb;
}

uint8_t *gfx_fb_mut(void)
{
    return s_fb;
}

void gfx_fb_load(const uint8_t *fb)
{
    memcpy(s_fb, fb, OLED_FB_SIZE);
}

void gfx_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
}

static inline void fb_set(int x, int y, bool on)
{
    if ((unsigned)x >= OLED_W || (unsigned)y >= OLED_H) {
        return;
    }
    uint8_t *p = &s_fb[((y >> 3) * OLED_W) + x];
    uint8_t mask = (uint8_t)(1u << (y & 7));
    if (on) {
        *p |= mask;
    } else {
        *p &= (uint8_t)~mask;
    }
}

void gfx_pixel(int x, int y, bool on)
{
    fb_set(x, y, on);
}

void gfx_hline(int x, int y, int w, bool on)
{
    for (int i = 0; i < w; i++) {
        fb_set(x + i, y, on);
    }
}

void gfx_vline(int x, int y, int h, bool on)
{
    for (int i = 0; i < h; i++) {
        fb_set(x, y + i, on);
    }
}

void gfx_rect(int x, int y, int w, int h)
{
    gfx_hline(x, y, w, true);
    gfx_hline(x, y + h - 1, w, true);
    gfx_vline(x, y, h, true);
    gfx_vline(x + w - 1, y, h, true);
}

void gfx_fill_rect(int x, int y, int w, int h, bool on)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            fb_set(x + i, y + j, on);
        }
    }
}

void gfx_invert_rect(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = x + i;
            int py = y + j;
            if ((unsigned)px >= OLED_W || (unsigned)py >= OLED_H) {
                continue;
            }
            s_fb[((py >> 3) * OLED_W) + px] ^= (uint8_t)(1u << (py & 7));
        }
    }
}

/* 字形数据是行优先 MSB-left 的位图，逐位展开到 page 格式帧缓冲 */
void gfx_blit(int x, int y, int w, int h, const uint8_t *packed)
{
    int bpr = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        int py = y + row;
        if ((unsigned)py >= OLED_H) {
            continue;
        }
        const uint8_t *line = packed + row * bpr;
        for (int col = 0; col < w; col++) {
            if (line[col >> 3] & (0x80 >> (col & 7))) {
                fb_set(x + col, py, true);
            }
        }
    }
}

/* 按码点挑字体：8px 符号优先，其次 16px 汉字 */
static const uint8_t *pick_glyph(uint32_t cp, const ui_font_t **font)
{
    const uint8_t *g = ui_font_glyph(&font_ascii_8x16, cp);
    if (g) {
        *font = &font_ascii_8x16;
        return g;
    }
    g = ui_font_glyph(&font_cn_16x16, cp);
    if (g) {
        *font = &font_cn_16x16;
        return g;
    }
    *font = &font_ascii_8x16;
    return ui_font_glyph(&font_ascii_8x16, '?');
}

int gfx_text_max(int x, int y, int maxw, const char *utf8)
{
    if (utf8 == NULL) {
        return 0;
    }
    int x0 = x;
    const char *p = utf8;
    while (*p) {
        uint32_t cp = ui_utf8_next(&p);
        if (cp == 0) {
            break;
        }
        const ui_font_t *f;
        const uint8_t *g = pick_glyph(cp, &f);
        if (g == NULL) {
            continue;
        }
        /* 同时受调用方限宽和屏幕右边界约束，避免文字画到屏幕外 */
        if (x + f->w > x0 + maxw || x + f->w > OLED_W) {
            break;
        }
        gfx_blit(x, y, f->w, f->h, g);
        x += f->w;
    }
    return x - x0;
}

int gfx_text(int x, int y, const char *utf8)
{
    return gfx_text_max(x, y, OLED_W - x, utf8);
}

int gfx_text_width(const char *utf8)
{
    if (utf8 == NULL) {
        return 0;
    }
    int w = 0;
    const char *p = utf8;
    while (*p) {
        uint32_t cp = ui_utf8_next(&p);
        if (cp == 0) {
            break;
        }
        const ui_font_t *f;
        if (ui_font_glyph(&font_ascii_8x16, cp)) {
            f = &font_ascii_8x16;
        } else if (ui_font_glyph(&font_cn_16x16, cp)) {
            f = &font_cn_16x16;
        } else {
            f = &font_ascii_8x16;
        }
        w += f->w;
    }
    return w;
}

int gfx_num(int x, int y, const char *ascii)
{
    if (ascii == NULL) {
        return 0;
    }
    int x0 = x;
    for (const char *p = ascii; *p; p++) {
        const uint8_t *g = ui_font_glyph(&font_num_24x32, (uint8_t)*p);
        if (g == NULL) {
            g = ui_font_glyph(&font_num_24x32, ' ');
        }
        if (g == NULL) {
            continue;
        }
        if (x + font_num_24x32.w > OLED_W) {
            break;
        }
        gfx_blit(x, y, font_num_24x32.w, font_num_24x32.h, g);
        x += font_num_24x32.w;
    }
    return x - x0;
}
