#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/*
 * SSD1306 128x64 I2C 驱动（ESP-IDF v6.0 新版 i2c_master 驱动）
 * 显存格式与 gfx 帧缓冲一致，刷新即整块搬运。
 */

esp_err_t ssd1306_init(void);
esp_err_t ssd1306_flush(const uint8_t *fb);
esp_err_t ssd1306_set_contrast(uint8_t value);
esp_err_t ssd1306_display_on(bool on);
