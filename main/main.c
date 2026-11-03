#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "app_cfg.h"
#include "app_data.h"
#include "bili.h"
#include "buzzer.h"
#include "cfg.h"
#include "gfx.h"
#include "mqtt_app.h"
#include "prov.h"
#include "ssd1306.h"
#include "ui.h"
#include "weather.h"
#include "wifi.h"

static const char *TAG = "main";

/* 时间大于 2020 年才算对时成功 */
static bool time_ok(void)
{
    return time(NULL) > 1600000000;
}

static void show_screen(const char *l1, const char *l2, const char *l3)
{
    gfx_clear();
    if (l1 != NULL) {
        gfx_text(2, 0, l1);
    }
    if (l2 != NULL) {
        gfx_text(2, 20, l2);
    }
    if (l3 != NULL) {
        gfx_text(2, 40, l3);
    }
    ssd1306_flush(gfx_fb());
}

static void key_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_KEY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}

static void time_sync_start(void)
{
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_init();
}

/* 进入配网模式后不再返回（保存配置会重启） */
static void enter_prov_mode(void)
{
    prov_start();
    for (;;) {
        gfx_clear();
        gfx_text(2, 0, "配网模式");
        gfx_text(2, 20, "连接热点");
        gfx_text_max(2, 36, OLED_W - 4, wifi_ap_ssid());
        gfx_text(2, 52, "192.168.4.1");
        ssd1306_flush(gfx_fb());
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ---------------- 周期任务 ---------------- */
static void task_weather(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(3000));
    for (;;) {
        if (wifi_connected()) {
            weather_fetch();
        }
        vTaskDelay(pdMS_TO_TICKS(WEATHER_PERIOD_S * 1000));
    }
}

static void task_bili(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(6000));
    for (;;) {
        if (wifi_connected() && (g_cfg.bili_uid[0] != '\0' || g_cfg.bili_bv[0] != '\0')) {
            bili_fetch();
        }
        vTaskDelay(pdMS_TO_TICKS(BILI_PERIOD_S * 1000));
    }
}

/* 页面轮播序列：时钟页出现两次，节奏更自然 */
static const int SEQ[] = {UI_PAGE_CLOCK, UI_PAGE_WEATHER, UI_PAGE_CLOCK, UI_PAGE_BILI};

static int next_page(int *idx)
{
    for (int i = 0; i < (int)(sizeof(SEQ) / sizeof(SEQ[0])); i++) {
        *idx = (*idx + 1) % (int)(sizeof(SEQ) / sizeof(SEQ[0]));
        if (SEQ[*idx] == UI_PAGE_BILI && g_cfg.bili_uid[0] == '\0' && g_cfg.bili_bv[0] == '\0') {
            continue;
        }
        return SEQ[*idx];
    }
    return UI_PAGE_CLOCK;
}

static void task_render(void *arg)
{
    (void)arg;
    int seq_idx = 0;
    int page_timer = 0;
    int page = UI_PAGE_CLOCK;
    time_t msg_deadline = 0;
    uint32_t seen_serial = g_data.msg_serial;
    bool last_key = false;
    int key_hold = 0;

    for (;;) {
        time_t now = time(NULL);
        struct tm tmv;
        localtime_r(&now, &tmv);

        g_data.tm = tmv;
        g_data.time_valid = time_ok();
        g_data.wifi_connected = wifi_connected();
        g_data.mqtt_connected = mqtt_connected();

        /* BOOT 键：短按停闹钟，长按 5 秒清 WiFi 配置并重启进配网 */
        bool key = (gpio_get_level(PIN_KEY) == 0);
        if (key && !last_key && alarm_ringing()) {
            alarm_stop();
        }
        last_key = key;

        if (key) {
            if (++key_hold >= 5) {
                ESP_LOGW(TAG, "长按 BOOT 5 秒：清除 WiFi 配置，重启进入配网模式");
                g_cfg.wifi_ssid[0] = '\0';
                g_cfg.wifi_pass[0] = '\0';
                cfg_save();
                esp_restart();
            }
        } else {
            key_hold = 0;
        }

        alarm_tick(&tmv);

        /* 新消息 / 手机指定页面 → 临时弹屏 10 秒 */
        if (g_data.msg_serial != seen_serial) {
            seen_serial = g_data.msg_serial;
            page = UI_PAGE_MSG;
            msg_deadline = now + 10;
        }
        if (g_req_page >= 0) {
            page = g_req_page;
            g_req_page = -1;
            msg_deadline = now + 15;
        }
        if (msg_deadline != 0 && now >= msg_deadline) {
            msg_deadline = 0;
        }

        if (msg_deadline == 0) {
            if (++page_timer >= (int)g_cfg.page_sec) {
                page_timer = 0;
                page = next_page(&seq_idx);
            }
        }

        ui_render(page);
        ssd1306_flush(gfx_fb());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    cfg_init();
    data_init();
    buzzer_init();
    key_init();

    setenv("TZ", g_cfg.tz, 1);
    tzset();

    ESP_LOGI(TAG, "========== 物联网天气时钟 (ESP32-S2) ==========");

    bool oled_ok = (ssd1306_init() == ESP_OK);
    if (oled_ok) {
        ssd1306_set_contrast(g_cfg.brightness);
        show_screen("天气时钟", "启动中", NULL);
    } else {
        ESP_LOGE(TAG, "OLED 初始化失败，串口仍可正常使用");
    }

    if (!cfg_wifi_ready()) {
        ESP_LOGI(TAG, "尚未配置 WiFi，进入配网模式");
        enter_prov_mode();
    }

    wifi_start_sta();

    /* 最多等 20 秒，连不上就转配网，避免"换路由器后板子变砖" */
    for (int i = 0; i < 40 && !wifi_connected(); i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (!wifi_connected()) {
        ESP_LOGW(TAG, "20 秒内未连上 WiFi，转入配网模式");
        enter_prov_mode();
    }

    show_screen("天气时钟", "正在对时", wifi_sta_ip());
    time_sync_start();
    for (int i = 0; i < 60 && !time_ok(); i++) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (!time_ok()) {
        ESP_LOGW(TAG, "NTP 对时超时，稍后仍会自动同步");
    } else {
        ESP_LOGI(TAG, "对时完成");
    }

    mqtt_start();

    xTaskCreate(task_weather, "weather", 10240, NULL, 4, NULL);
    xTaskCreate(task_bili, "bili", 10240, NULL, 3, NULL);
    xTaskCreate(task_render, "render", 5120, NULL, 5, NULL);

    ESP_LOGI(TAG, "初始化完成，进入主循环");
}
