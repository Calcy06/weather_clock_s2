#include <stddef.h>

#include "font.h"

/*
 * 字形查找：码点表升序，二分查找。
 * 一帧最多查几百次，count 约 4400，12 次比较即可命中。
 */
const uint8_t *ui_font_glyph(const ui_font_t *f, uint32_t cp)
{
    if (f == NULL || cp > 0xFFFF) {
        return NULL;
    }
    uint16_t key = (uint16_t)cp;
    uint16_t lo = 0;
    uint16_t hi = f->count;
    while (lo < hi) {
        uint16_t mid = (uint16_t)(lo + (hi - lo) / 2);
        uint16_t v = f->cps[mid];
        if (v == key) {
            return f->data + (uint32_t)mid * f->bpr * f->h;
        }
        if (v < key) {
            lo = (uint16_t)(mid + 1);
        } else {
            hi = mid;
        }
    }
    return NULL;
}

uint32_t ui_utf8_next(const char **s)
{
    const uint8_t *p = (const uint8_t *)(*s);
    if (p[0] == 0) {
        return 0;
    }

    uint32_t cp;
    int n;
    if (p[0] < 0x80) {
        cp = p[0];
        n = 1;
    } else if ((p[0] & 0xE0) == 0xC0) {
        cp = p[0] & 0x1F;
        n = 2;
    } else if ((p[0] & 0xF0) == 0xE0) {
        cp = p[0] & 0x0F;
        n = 3;
    } else if ((p[0] & 0xF8) == 0xF0) {
        cp = p[0] & 0x07;
        n = 4;
    } else {
        *s = (const char *)(p + 1);
        return '?';
    }

    for (int i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) {   /* 非法续字节，按单字节跳过 */
            *s = (const char *)(p + i);
            return '?';
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *s = (const char *)(p + n);
    return cp;
}
