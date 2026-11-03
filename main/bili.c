#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

#include "app_data.h"
#include "bili.h"
#include "cfg.h"
#include "net_http.h"

static const char *TAG = "bili";

/* B 站对空 UA 会直接 412，必须带一个浏览器 UA */
static const char *UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

static char s_buf[3072];

/* 取 data 字段里的整型，成功返回 true */
static bool get_int(cJSON *data, const char *path_key, uint32_t *out)
{
    cJSON *it = cJSON_GetObjectItem(data, path_key);
    if (cJSON_IsNumber(it)) {
        *out = (uint32_t)it->valuedouble;
        return true;
    }
    return false;
}

esp_err_t bili_fetch(void)
{
    if (g_cfg.bili_uid[0] == '\0' && g_cfg.bili_bv[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    bool ok = false;
    char url[160];

    if (g_cfg.bili_uid[0] != '\0') {
        snprintf(url, sizeof(url), "https://api.bilibili.com/x/relation/stat?vmid=%s",
                 g_cfg.bili_uid);
        if (http_get_text(url, UA, s_buf, sizeof(s_buf)) == ESP_OK) {
            cJSON *root = cJSON_Parse(s_buf);
            if (root != NULL) {
                cJSON *code = cJSON_GetObjectItem(root, "code");
                cJSON *data = cJSON_GetObjectItem(root, "data");
                if (cJSON_IsNumber(code) && code->valueint == 0 && data != NULL) {
                    uint32_t fans = 0;
                    if (get_int(data, "follower", &fans)) {
                        g_data.bili_fans = fans;
                        ok = true;
                    }
                } else {
                    ESP_LOGW(TAG, "粉丝接口返回 code=%d（UID 是否正确？）",
                             cJSON_IsNumber(code) ? code->valueint : -1);
                }
                cJSON_Delete(root);
            }
        }
    }

    if (g_cfg.bili_bv[0] != '\0') {
        snprintf(url, sizeof(url), "https://api.bilibili.com/x/web-interface/view?bvid=%s",
                 g_cfg.bili_bv);
        if (http_get_text(url, UA, s_buf, sizeof(s_buf)) == ESP_OK) {
            cJSON *root = cJSON_Parse(s_buf);
            if (root != NULL) {
                cJSON *code = cJSON_GetObjectItem(root, "code");
                cJSON *data = cJSON_GetObjectItem(root, "data");
                if (cJSON_IsNumber(code) && code->valueint == 0 && data != NULL) {
                    cJSON *stat = cJSON_GetObjectItem(data, "stat");
                    uint32_t views = 0;
                    if (stat != NULL && get_int(stat, "view", &views)) {
                        g_data.bili_views = views;
                        ok = true;
                    }
                } else {
                    ESP_LOGW(TAG, "视频接口返回 code=%d（BV 号是否正确？）",
                             cJSON_IsNumber(code) ? code->valueint : -1);
                }
                cJSON_Delete(root);
            }
        }
    }

    if (ok) {
        strncpy(g_data.bili_bv, g_cfg.bili_bv, sizeof(g_data.bili_bv) - 1);
        g_data.bili_valid = true;
        ESP_LOGI(TAG, "粉丝 %u  播放 %u", (unsigned)g_data.bili_fans,
                 (unsigned)g_data.bili_views);
        return ESP_OK;
    }
    return ESP_FAIL;
}
