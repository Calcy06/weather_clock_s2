#include "driver/gpio.h"
#include "esp_log.h"

#include "app_cfg.h"
#include "buzzer.h"
#include "cfg.h"

static const char *TAG = "buzzer";

static bool s_ringing;
static int s_ring_sec;
static uint8_t s_fired_day = 0xFF;   /* 当天已响过就不再重复 */

void buzzer_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(PIN_BUZZER, 0);
}

void buzzer_set(bool on)
{
    /* 有源蜂鸣器：高电平即响，无需 PWM */
    gpio_set_level(PIN_BUZZER, on ? 1 : 0);
}

void alarm_stop(void)
{
    if (s_ringing) {
        ESP_LOGI(TAG, "闹钟已停止");
    }
    s_ringing = false;
    s_ring_sec = 0;
    buzzer_set(false);
}

bool alarm_ringing(void)
{
    return s_ringing;
}

void alarm_tick(const struct tm *t)
{
    if (s_ringing) {
        s_ring_sec++;
        if (s_ring_sec >= ALARM_RING_S) {
            ESP_LOGI(TAG, "闹钟响满 %d 秒，自动停止", ALARM_RING_S);
            alarm_stop();
            return;
        }
        /* 1 秒周期通断，比长鸣更容易引起注意 */
        buzzer_set((s_ring_sec & 1) == 0);
        return;
    }

    if (!g_cfg.alarm_en) {
        return;
    }
    if (t->tm_hour == g_cfg.alarm_hh && t->tm_min == g_cfg.alarm_mm && t->tm_sec < 2 &&
        s_fired_day != (uint8_t)t->tm_mday) {
        s_fired_day = (uint8_t)t->tm_mday;
        s_ringing = true;
        s_ring_sec = 0;
        buzzer_set(true);
        ESP_LOGI(TAG, "闹钟响铃 %02u:%02u", (unsigned)g_cfg.alarm_hh, (unsigned)g_cfg.alarm_mm);
    }
}
