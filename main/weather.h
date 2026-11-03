#pragma once

#include "esp_err.h"

/* 请求心知天气（now + daily），成功返回 ESP_OK 并更新 g_data */
esp_err_t weather_fetch(void);
