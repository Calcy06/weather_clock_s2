#include <stdio.h>
#include <string.h>

#include "app_cfg.h"
#include "app_data.h"
#include "cfg.h"
#include "gfx.h"
#include "ui.h"

app_data_t g_data;

void data_init(void)
{
    memset(&g_data, 0, sizeof(g_data));
    strcpy(g_data.w_text, "晴");
    strcpy(g_data.w_temp, "--");
    strcpy(g_data.w_low, "--");
    strcpy(g_data.w_high, "--");
    strcpy(g_data.w_wind, "--");
}

static const char *const WK_NAME[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

/* 右对齐绘制，返回起始 x */
static void text_right(int right_x, int y, const char *s)
{
    int w = gfx_text_width(s);
    gfx_text(right_x - w, y, s);
}

/* ---------------- 页面 0：时钟主页 ---------------- */
static void page_clock(void)
{
    char buf[48];
    const struct tm *t = &g_data.tm;

    if (g_data.time_valid) {
        snprintf(buf, sizeof(buf), "%d月%02d日%s", t->tm_mon + 1, t->tm_mday,
                 WK_NAME[t->tm_wday % 7]);
        gfx_text(2, 0, buf);

        snprintf(buf, sizeof(buf), "%02d", t->tm_sec);
        text_right(OLED_W - 2, 0, buf);

        snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
        gfx_num(4, 16, buf);
    } else {
        gfx_text(2, 0, "等待网络对时");
        gfx_num(4, 16, "--:--");
    }

    if (g_data.weather_valid) {
        /* 三区固定布局：天气(2..34) | 温度(36..) | 风向风力(右对齐)
         *
         * 风向风力长度不可控（"北3级" 40px，"东北偏北12级" 96px），
         * 温度带负号也会变宽（"23°C" 32px，"-12°C" 40px）。
         * 两者同时变长会重叠糊成一团，所以风向只在放得下时才画，
         * 放不下就省略 —— 天气详情页始终有完整风向。
         */
        gfx_text_max(2, 48, 32, g_data.w_text);
        snprintf(buf, sizeof(buf), "%s°C", g_data.w_temp);
        int temp_end = 36 + gfx_text_max(36, 48, 44, buf);

        int zone_l = temp_end + 4;          /* 风向最早可以从这里开始 */
        int zone_r = OLED_W - 2;            /* 126 */
        int wind_w = gfx_text_width(g_data.w_wind);
        if (wind_w > 0 && zone_l + wind_w <= zone_r) {
            gfx_text(zone_r - wind_w, 48, g_data.w_wind);
        }
    } else {
        gfx_text(2, 48, g_data.wifi_connected ? "正在获取天气" : "正在连接网络");
    }
}

/* ---------------- 页面 1：天气详情 ---------------- */
static void page_weather(void)
{
    char buf[48];

    if (!g_data.weather_valid) {
        gfx_text(2, 0, "天气");
        gfx_text(2, 24, g_data.wifi_connected ? "暂无数据" : "网络未连接");
        return;
    }

    snprintf(buf, sizeof(buf), "%s %s", g_cfg.sen_loc, g_data.w_text);
    gfx_text_max(2, 0, OLED_W - 4, buf);

    snprintf(buf, sizeof(buf), "%s°C", g_data.w_temp);
    gfx_text(2, 16, buf);
    snprintf(buf, sizeof(buf), "%s~%s°C", g_data.w_low, g_data.w_high);
    gfx_text(48, 16, buf);

    /* 免费版可取字段：风向风力（湿度需付费权限，本项目不显示） */
    gfx_text_max(2, 32, OLED_W - 4, g_data.w_wind);

    snprintf(buf, sizeof(buf), "更新%02d:%02d", g_data.w_upd_h, g_data.w_upd_m);
    gfx_text(2, 48, buf);
}

/* ---------------- 页面 2：哔哩信息 ---------------- */
static void page_bili(void)
{
    char buf[48];

    gfx_text(2, 0, "B站 UP主");
    if (!g_data.bili_valid) {
        gfx_text(2, 24, g_data.wifi_connected ? "暂无数据" : "网络未连接");
        return;
    }
    snprintf(buf, sizeof(buf), "粉丝 %u", (unsigned)g_data.bili_fans);
    gfx_text_max(2, 16, OLED_W - 4, buf);
    snprintf(buf, sizeof(buf), "播放 %u", (unsigned)g_data.bili_views);
    gfx_text_max(2, 32, OLED_W - 4, buf);
    gfx_text_max(2, 48, OLED_W - 4, g_data.bili_bv);
}

/* ---------------- 页面 3：手机消息 ---------------- */
static void page_msg(void)
{
    char buf[48];

    gfx_text(2, 0, "消息");
    snprintf(buf, sizeof(buf), "%02d:%02d", g_data.msg_upd_h, g_data.msg_upd_m);
    text_right(OLED_W - 2, 0, buf);

    if (g_data.msg_count == 0) {
        gfx_text(2, 24, "暂无消息");
        return;
    }
    for (int i = 0; i < 3 && i < g_data.msg_count; i++) {
        gfx_text_max(2, 16 + i * 16, OLED_W - 4, g_data.msg[i]);
    }
}

void ui_render(int page)
{
    gfx_clear();
    switch (page) {
    case UI_PAGE_WEATHER:
        page_weather();
        break;
    case UI_PAGE_BILI:
        page_bili();
        break;
    case UI_PAGE_MSG:
        page_msg();
        break;
    case UI_PAGE_CLOCK:
    default:
        page_clock();
        break;
    }
}
