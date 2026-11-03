#pragma once

#include <stdbool.h>

#include "esp_err.h"

/*
 * 网页配网：启动 AP 热点 + HTTP 服务，手机连上后打开 http://192.168.4.1
 * 填写 WiFi / 心知天气 / B站 / MQTT / 闹钟等配置，保存后设备自动重启。
 */
esp_err_t prov_start(void);

/* 配网服务是否正在运行 */
bool prov_running(void);
