#pragma once

#include <stdint.h>

/*
 * 1-bit 点阵字体描述（由 tools/gen_font.py 生成）
 *
 * 数据格式：行优先，每行 bpr 字节，MSB 在左，1 = 点亮。
 * cps 为按升序排列的 Unicode 码点表，第 i 个码点的字形位于 data + i*bpr*h。
 */
typedef struct {
    const uint8_t *data;    /* count * bpr * h 字节点阵 */
    const char *chars;      /* UTF-8 字符表（调试/参考用） */
    const uint16_t *cps;    /* 升序码点表，二分查找 */
    uint8_t w;              /* 字形宽（像素） */
    uint8_t h;              /* 字形高（像素） */
    uint8_t bpr;            /* 每行字节数 */
    uint16_t count;         /* 字形个数 */
} ui_font_t;

extern const ui_font_t font_ascii_8x16;   /* 宽 8  */
extern const ui_font_t font_num_24x32;    /* 宽 24，大字时间 */
extern const ui_font_t font_cn_16x16;     /* 宽 16，GB2312 汉字 */

/* 按 Unicode 码点查字形，未收录返回 NULL */
const uint8_t *ui_font_glyph(const ui_font_t *f, uint32_t cp);

/* UTF-8 解码：返回码点，*s 自动前移；非法字节按单字节 '?' 处理 */
uint32_t ui_utf8_next(const char **s);
