#pragma once

/*
 * 硬件引脚与全局常量 —— ESP32-S2-DevKitC-1-N8R2
 *
 * 引脚选择理由：
 *   GPIO19/20 是 USB D-/D+，GPIO26-32 接内部 Flash，均不可用
 *   GPIO0/45/46 是 strapping 脚，GPIO0 恰好是板载 BOOT 键，正好拿来做"停止闹钟"键
 *   I2C 用 GPIO8/9 —— 两者在 DevKitC-1 排针上都有引出，且不与 Flash/USB 冲突
 */

#define PIN_I2C_SDA     8       /* OLED SDA */
#define PIN_I2C_SCL     9       /* OLED SCL */
#define PIN_BUZZER      5       /* 有源蜂鸣器信号脚（高电平响） */
#define PIN_KEY         0       /* 板载 BOOT 键，低电平按下 */

#define OLED_ADDR       0x3C    /* SSD1306 常见地址；若屏不亮试 0x3D */
#define OLED_W          128
#define OLED_H          64
#define OLED_FB_SIZE    (OLED_W * OLED_H / 8)

#define APP_TAG         "WC"

/* 时间/天气刷新周期（秒） */
#define WEATHER_PERIOD_S    900     /* 心知天气免费额度有限，15 分钟一次 */
#define BILI_PERIOD_S       600
#define NTP_PERIOD_S        1800
#define PAGE_ROTATE_S       6       /* 非时钟页轮播间隔 */

/* 配网 AP */
#define PROV_AP_PREFIX      "WC-"

/* 闹钟响铃时长（秒），按 BOOT 键可提前停 */
#define ALARM_RING_S        60
