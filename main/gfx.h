#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_cfg.h"
#include "font.h"

/*
 * 单色帧缓冲绘图。
 * 缓冲布局直接对齐 SSD1306 显存：fb[(y>>3)*128 + x] 的 bit(y&7)，1 = 点亮。
 * 这样刷新时无需重排，一次 I2C 传输即可。
 */

void gfx_clear(void);
void gfx_pixel(int x, int y, bool on);
void gfx_hline(int x, int y, int w, bool on);
void gfx_vline(int x, int y, int h, bool on);
void gfx_rect(int x, int y, int w, int h);              /* 描边 */
void gfx_fill_rect(int x, int y, int w, int h, bool on);
void gfx_invert_rect(int x, int y, int w, int h);
void gfx_blit(int x, int y, int w, int h, const uint8_t *packed);

/* 绘制 UTF-8 文本：ASCII/符号用 8x16，汉字用 16x16。返回实际宽度 */
int gfx_text(int x, int y, const char *utf8);
/* 带最大宽度限制，超出部分直接截断 */
int gfx_text_max(int x, int y, int maxw, const char *utf8);
/* 24x32 大号数字（时间用） */
int gfx_num(int x, int y, const char *ascii);

int gfx_text_width(const char *utf8);

const uint8_t *gfx_fb(void);
uint8_t *gfx_fb_mut(void);
void gfx_fb_load(const uint8_t *fb);   /* 测试用：整块覆盖 */
