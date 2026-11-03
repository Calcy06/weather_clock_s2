/*
 * 主机侧渲染测试 —— 用 PC 的 gcc 编译真正的 gfx/font/ui 代码，
 * 把 4 个页面导出成 PGM 图像，用于和 Python 预览图比对。
 * 不参与固件编译。
 */
#include <stdio.h>
#include <string.h>

#include "app_cfg.h"
#include "app_data.h"
#include "cfg.h"
#include "gfx.h"
#include "ui.h"

cfg_t g_cfg;   /* 测试时代替 cfg.c 里的定义 */

static void dump_pgm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "无法写入 %s\n", path);
        return;
    }
    fprintf(f, "P5\n%d %d\n255\n", OLED_W, OLED_H);
    const uint8_t *fb = gfx_fb();
    for (int y = 0; y < OLED_H; y++) {
        for (int x = 0; x < OLED_W; x++) {
            uint8_t on = fb[((y >> 3) * OLED_W) + x] & (1u << (y & 7));
            fputc(on ? 255 : 0, f);
        }
    }
    fclose(f);
}

int main(void)
{
    data_init();

    g_data.time_valid = true;
    g_data.wifi_connected = true;
    g_data.mqtt_connected = true;
    g_data.tm.tm_year = 126;
    g_data.tm.tm_mon = 10;      /* 11 月 */
    g_data.tm.tm_mday = 3;
    g_data.tm.tm_wday = 2;      /* 周二 */
    g_data.tm.tm_hour = 9;
    g_data.tm.tm_min = 41;
    g_data.tm.tm_sec = 41;

    g_data.weather_valid = true;
    strcpy(g_data.w_text, "晴");
    strcpy(g_data.w_temp, "23");
    strcpy(g_data.w_low, "12");
    strcpy(g_data.w_high, "23");
    strcpy(g_data.w_wind, "北3级");
    g_data.w_upd_h = 19;
    g_data.w_upd_m = 20;

    g_data.bili_valid = true;
    g_data.bili_fans = 128000;
    g_data.bili_views = 3500000;
    strcpy(g_data.bili_bv, "BV1L7411c7jw");

    g_data.msg_count = 3;
    strcpy(g_data.msg[0], "收到一条消息");
    strcpy(g_data.msg[1], "明早7点带伞");
    strcpy(g_data.msg[2], "闹钟06:30");
    g_data.msg_upd_h = 19;
    g_data.msg_upd_m = 20;

    strcpy(g_cfg.sen_loc, "beijing");
    g_cfg.brightness = 0xCF;
    g_cfg.page_sec = 6;

    const char *names[UI_PAGE_COUNT] = {"clock", "weather", "bili", "msg"};
    char path[64];
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        ui_render(i);
        snprintf(path, sizeof(path), "docs/host_%s.pgm", names[i]);
        dump_pgm(path);
        printf("渲染 %-8s -> %s\n", names[i], path);
    }

    /* 边界场景：未配网 / 未对时 / 无天气 / 无B站 / 无消息 */
    data_init();
    g_data.time_valid = false;
    g_data.wifi_connected = false;
    g_data.weather_valid = false;
    g_data.bili_valid = false;
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        ui_render(i);
        snprintf(path, sizeof(path), "docs/edge_%s.pgm", names[i]);
        dump_pgm(path);
        printf("边界 %-8s -> %s\n", names[i], path);
    }

    /* 极端数据：最长天气词、负温度、7 位粉丝数、10 位播放量、超长混合文本 */
    g_data.time_valid = true;
    g_data.wifi_connected = true;
    g_data.tm.tm_mon = 11;
    g_data.tm.tm_mday = 31;
    g_data.tm.tm_wday = 4;
    g_data.tm.tm_hour = 23;
    g_data.tm.tm_min = 59;
    g_data.tm.tm_sec = 59;
    g_data.weather_valid = true;
    strncpy(g_data.w_text, "强沙尘暴", sizeof(g_data.w_text) - 1);
    strcpy(g_data.w_temp, "-12");
    strcpy(g_data.w_low, "-15");
    strcpy(g_data.w_high, "-8");
    strncpy(g_data.w_wind, "东北偏北12级", sizeof(g_data.w_wind) - 1);
    g_data.bili_valid = true;
    g_data.bili_fans = 9999999;
    g_data.bili_views = 1234567890;
    g_data.msg_count = 3;
    strncpy(g_data.msg[0], "这是一条特别特别长的中文消息用来测试截断", sizeof(g_data.msg[0]) - 1);
    g_data.msg[0][sizeof(g_data.msg[0]) - 1] = 0;
    strncpy(g_data.msg[1], "Mixed中英文messagelikethis", sizeof(g_data.msg[1]) - 1);
    g_data.msg[1][sizeof(g_data.msg[1]) - 1] = 0;
    strncpy(g_data.msg[2], "0123456789012345678901234567890123456789", sizeof(g_data.msg[2]) - 1);
    g_data.msg[2][sizeof(g_data.msg[2]) - 1] = 0;
    for (int i = 0; i < UI_PAGE_COUNT; i++) {
        ui_render(i);
        snprintf(path, sizeof(path), "docs/edge2_%s.pgm", names[i]);
        dump_pgm(path);
        printf("极端 %-8s -> %s\n", names[i], path);
    }
    return 0;
}
