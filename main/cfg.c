#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "cfg.h"

static const char *TAG = "cfg";

#define CFG_NS      "wc"
#define CFG_KEY     "cfg"
#define CFG_MAGIC   0x57433031u   /* "WC01" */
#define CFG_VER     1

typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t rsv;
    cfg_t v;
} cfg_blob_t;

cfg_t g_cfg;

static void set_defaults(cfg_t *c)
{
    memset(c, 0, sizeof(*c));
    strcpy(c->mqtt_uri, "mqtt://test.ranye-iot.net:1883");
    strcpy(c->tz, "CST-8");
    c->brightness = 0xCF;
    c->page_sec = 6;
    c->alarm_hh = 7;
    c->alarm_mm = 0;
    c->alarm_en = 0;
}

void cfg_init(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(CFG_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open 失败: %s", esp_err_to_name(err));
        set_defaults(&g_cfg);
        return;
    }

    cfg_blob_t blob;
    size_t len = sizeof(blob);
    err = nvs_get_blob(h, CFG_KEY, &blob, &len);
    if (err == ESP_OK && len == sizeof(blob) && blob.magic == CFG_MAGIC && blob.ver == CFG_VER) {
        g_cfg = blob.v;
        /* 防御：NVS 内容被外部写坏时兜底 */
        g_cfg.wifi_ssid[sizeof(g_cfg.wifi_ssid) - 1] = '\0';
        g_cfg.mqtt_uri[sizeof(g_cfg.mqtt_uri) - 1] = '\0';
        g_cfg.tz[sizeof(g_cfg.tz) - 1] = '\0';
        if (g_cfg.brightness == 0) {
            g_cfg.brightness = 0xCF;
        }
        if (g_cfg.page_sec < 3) {
            g_cfg.page_sec = 6;
        }
        ESP_LOGI(TAG, "配置已加载 (ssid=%s)", g_cfg.wifi_ssid[0] ? g_cfg.wifi_ssid : "<未配置>");
    } else {
        set_defaults(&g_cfg);
        cfg_save();
        ESP_LOGI(TAG, "首次启动，写入默认配置");
    }
    nvs_close(h);
}

void cfg_save(void)
{
    nvs_handle_t h;
    if (nvs_open(CFG_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    cfg_blob_t blob = {.magic = CFG_MAGIC, .ver = CFG_VER, .rsv = 0, .v = g_cfg};
    esp_err_t err = nvs_set_blob(h, CFG_KEY, &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存配置失败: %s", esp_err_to_name(err));
    }
    nvs_close(h);
}

void cfg_reset(void)
{
    set_defaults(&g_cfg);
    cfg_save();
}

bool cfg_wifi_ready(void)
{
    return g_cfg.wifi_ssid[0] != '\0';
}
