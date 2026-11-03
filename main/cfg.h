#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 全部可配置项，存于 NVS，由网页配网页面修改 */
typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char sen_key[40];      /* 心知天气私钥 */
    char sen_loc[24];      /* 心知天气城市，如 beijing */
    char bili_uid[16];     /* B 站 UP 主 uid，留空则不请求 */
    char bili_bv[16];      /* 视频 BV 号，留空则不请求 */
    char mqtt_uri[96];     /* mqtt://host:port */
    char mqtt_user[32];
    char mqtt_pass[32];
    char tz[24];           /* POSIX 时区串，CST-8 */
    uint8_t brightness;    /* 1..255 */
    uint8_t page_sec;      /* 页面轮播秒数 */
    uint8_t alarm_hh;      /* 闹钟 时 */
    uint8_t alarm_mm;      /* 闹钟 分 */
    uint8_t alarm_en;      /* 闹钟开关 */
} cfg_t;

extern cfg_t g_cfg;

void cfg_init(void);              /* 从 NVS 读取，失败则写入默认值 */
void cfg_save(void);              /* 写回 NVS */
void cfg_reset(void);             /* 恢复默认并保存 */
bool cfg_wifi_ready(void);        /* SSID 非空才算配过网 */
