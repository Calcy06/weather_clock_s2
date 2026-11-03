#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

/* WiFi 初始化（只做一次：netif / event loop / esp_wifi_init） */
esp_err_t wifi_init(void);

/* 以 STA 模式连接 cfg 中保存的 WiFi，断线自动重连 */
esp_err_t wifi_start_sta(void);

/* 以 AP+STA 模式启动配网热点 */
esp_err_t wifi_start_ap(void);

bool wifi_connected(void);
const char *wifi_sta_ip(void);
const char *wifi_ap_ssid(void);

/* 扫描周围热点，返回数量（调用前需已 wifi_start_sta/ap） */
int wifi_scan(wifi_ap_record_t *out, int max);
