#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t mqtt_start(void);
bool mqtt_connected(void);

/* 主动上报一次运行状态 */
void mqtt_report(void);

/* 三个主题名，供串口打印/文档使用 */
const char *mqtt_topic_control(void);
const char *mqtt_topic_chat(void);
const char *mqtt_topic_report(void);
