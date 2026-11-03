#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* 各网络任务共享的显示数据，读写都在主渲染任务里，无需加锁 */

typedef struct {
    bool time_valid;          /* SNTP 是否已对时 */
    struct tm tm;             /* 本地时间（已按 cfg.tz 转换） */

    bool wifi_connected;
    bool mqtt_connected;

    bool weather_valid;       /* 天气数据是否可用 */
    char w_text[24];          /* 天气现象，如 "晴" "多云" */
    char w_temp[8];           /* 实时温度 */
    char w_low[8];            /* 今日最低 */
    char w_high[8];           /* 今日最高 */
    char w_wind[24];          /* 风向风力，如 "东南3级"；最长"无持续风向4-5级"21字节 */
    uint8_t w_upd_h, w_upd_m; /* 数据更新时间 */

    bool bili_valid;
    uint32_t bili_fans;
    uint32_t bili_views;
    char bili_bv[16];

    char msg[3][28];          /* 最近 3 条 MQTT 消息 */
    uint8_t msg_count;
    uint8_t msg_upd_h, msg_upd_m;
    volatile uint32_t msg_serial;   /* 每收到一条自增，用于触发弹屏 */
} app_data_t;

extern app_data_t g_data;

void data_init(void);

/* 手机下发的页面切换请求（-1 = 无），由 MQTT 任务写、渲染任务消费 */
extern volatile int g_req_page;
