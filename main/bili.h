#pragma once

#include "esp_err.h"

/* 请求 B 站粉丝数与视频播放量，成功返回 ESP_OK 并更新 g_data */
esp_err_t bili_fetch(void);
