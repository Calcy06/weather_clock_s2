#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "app_cfg.h"
#include "cfg.h"
#include "wifi.h"

static const char *TAG = "wifi";

#define RETRY_DELAY_US  (3 * 1000 * 1000)

static bool s_connected;
static bool s_inited;
static bool s_started;
static esp_netif_t *s_netif_sta;
static esp_timer_handle_t s_retry_timer;
static char s_ip[16] = "0.0.0.0";
static char s_ap_ssid[40];

static void retry_cb(void *arg)
{
    (void)arg;
    if (!s_connected) {
        esp_wifi_connect();
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)data;
        s_connected = false;
        strcpy(s_ip, "0.0.0.0");
        ESP_LOGW(TAG, "连接断开 (reason=%d)，3 秒后重试", e->reason);
        /* 事件回调里不能阻塞，用一次性定时器延后重连 */
        esp_timer_stop(s_retry_timer);
        esp_timer_start_once(s_retry_timer, RETRY_DELAY_US);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        ESP_LOGI(TAG, "已获取 IP: %s", s_ip);
    }
}

esp_err_t wifi_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_netif_sta = esp_netif_create_default_wifi_sta();
    if (s_netif_sta == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init 失败: %s", esp_err_to_name(err));
        return err;
    }

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL);

    const esp_timer_create_args_t targs = {
        .callback = retry_cb,
        .name = "wifi_retry",
    };
    err = esp_timer_create(&targs, &s_retry_timer);
    if (err != ESP_OK) {
        return err;
    }

    s_inited = true;
    return ESP_OK;
}

esp_err_t wifi_start_sta(void)
{
    esp_err_t err = wifi_init();
    if (err != ESP_OK) {
        return err;
    }

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, g_cfg.wifi_ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, g_cfg.wifi_pass, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = (g_cfg.wifi_pass[0] == '\0') ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_STA, &wc);
    }
    if (err == ESP_OK && !s_started) {
        err = esp_wifi_start();
        if (err == ESP_OK) {
            s_started = true;
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动 STA 失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 关闭省电，保证 MQTT 长连接响应及时 */
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "正在连接 WiFi: %s", g_cfg.wifi_ssid);
    return ESP_OK;
}

esp_err_t wifi_start_ap(void)
{
    esp_err_t err = wifi_init();
    if (err != ESP_OK) {
        return err;
    }

    /* AP 名称用 MAC 后两字节区分，避免邻居互相干扰 */
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), PROV_AP_PREFIX "%02X%02X", mac[4], mac[5]);

    esp_netif_create_default_wifi_ap();

    wifi_config_t ac = {0};
    strncpy((char *)ac.ap.ssid, s_ap_ssid, sizeof(ac.ap.ssid) - 1);
    ac.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);
    ac.ap.channel = 1;
    ac.ap.max_connection = 2;
    ac.ap.authmode = WIFI_AUTH_OPEN;

    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &ac);
    }
    if (err == ESP_OK && !s_started) {
        err = esp_wifi_start();
        if (err == ESP_OK) {
            s_started = true;
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动配网热点失败: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "配网热点已开启: %s  打开 http://192.168.4.1", s_ap_ssid);
    return ESP_OK;
}

bool wifi_connected(void)
{
    return s_connected;
}

const char *wifi_sta_ip(void)
{
    return s_ip;
}

const char *wifi_ap_ssid(void)
{
    return s_ap_ssid;
}

int wifi_scan(wifi_ap_record_t *out, int max)
{
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        return 0;
    }
    uint16_t num = (uint16_t)max;
    if (esp_wifi_scan_get_ap_records(&num, out) != ESP_OK) {
        return 0;
    }
    return (int)num;
}
