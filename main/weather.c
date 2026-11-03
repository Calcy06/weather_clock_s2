#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"

#include "cJSON.h"

#include "app_data.h"
#include "cfg.h"
#include "net_http.h"
#include "weather.h"

static const char *TAG = "weather";

/* 心知免费版返回体不到 1KB，2KB 缓冲足够 */
static char s_buf[2048];

static void copy_str(cJSON *obj, const char *key, char *dst, size_t dstlen)
{
    cJSON *it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(it) && it->valuestring != NULL) {
        strncpy(dst, it->valuestring, dstlen - 1);
        dst[dstlen - 1] = '\0';
    }
}

static cJSON *first_result(void)
{
    cJSON *root = cJSON_Parse(s_buf);
    if (root == NULL) {
        return NULL;
    }
    cJSON *results = cJSON_GetObjectItem(root, "results");
    cJSON *r0 = cJSON_GetArrayItem(results, 0);
    if (r0 == NULL) {
        cJSON_Delete(root);
        return NULL;
    }
    /* 返回 root，调用方解析完后用 cJSON_Delete 释放 */
    return root;
}

esp_err_t weather_fetch(void)
{
    if (g_cfg.sen_key[0] == '\0' || g_cfg.sen_loc[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    char url[256];
    char text[24] = {0};
    char temp[8] = {0};
    char low[8] = {0};
    char high[8] = {0};
    char wdir[16] = {0};
    char wscale[8] = {0};

    /* 1) 实时天气：天气现象 + 实时温度 */
    snprintf(url, sizeof(url),
             "https://api.seniverse.com/v3/weather/now.json?key=%s&location=%s"
             "&language=zh-Hans&unit=c",
             g_cfg.sen_key, g_cfg.sen_loc);
    if (http_get_text(url, NULL, s_buf, sizeof(s_buf)) != ESP_OK) {
        return ESP_FAIL;
    }
    cJSON *root = first_result();
    if (root == NULL) {
        ESP_LOGW(TAG, "now.json 解析失败");
        return ESP_FAIL;
    }
    cJSON *results = cJSON_GetObjectItem(root, "results");
    cJSON *now = cJSON_GetObjectItem(cJSON_GetArrayItem(results, 0), "now");
    if (now != NULL) {
        copy_str(now, "text", text, sizeof(text));
        copy_str(now, "temperature", temp, sizeof(temp));
    }
    cJSON_Delete(root);

    if (text[0] == '\0') {
        ESP_LOGW(TAG, "心知返回异常，检查私钥/城市是否正确");
        return ESP_FAIL;
    }

    /* 2) 今日预报：最低/最高温、风向风力
     *
     * 心知免费版权限（0 元永久，10 万次/月）：
     *   - now.json   → 只返回 天气现象文字 / 天气代码 / 气温 3 项
     *   - daily.json → 3 天预报，含 最高/最低温 + 风向风速
     *   - humidity（湿度）属于付费权限，免费版拿不到，本项目也不显示
     */
    snprintf(url, sizeof(url),
             "https://api.seniverse.com/v3/weather/daily.json?key=%s&location=%s"
             "&language=zh-Hans&unit=c&start=0&days=1",
             g_cfg.sen_key, g_cfg.sen_loc);
    if (http_get_text(url, NULL, s_buf, sizeof(s_buf)) == ESP_OK) {
        root = first_result();
        if (root != NULL) {
            results = cJSON_GetObjectItem(root, "results");
            cJSON *daily = cJSON_GetObjectItem(cJSON_GetArrayItem(results, 0), "daily");
            cJSON *d0 = cJSON_GetArrayItem(daily, 0);
            if (d0 != NULL) {
                copy_str(d0, "low", low, sizeof(low));
                copy_str(d0, "high", high, sizeof(high));
                copy_str(d0, "wind_direction", wdir, sizeof(wdir));
                copy_str(d0, "wind_scale", wscale, sizeof(wscale));
            }
            cJSON_Delete(root);
        }
    }

    strncpy(g_data.w_text, text, sizeof(g_data.w_text) - 1);
    strncpy(g_data.w_temp, temp, sizeof(g_data.w_temp) - 1);
    if (low[0]) {
        strncpy(g_data.w_low, low, sizeof(g_data.w_low) - 1);
        strncpy(g_data.w_high, high, sizeof(g_data.w_high) - 1);
    }
    /* 风向风力：拼成 "东南3级" */
    if (wdir[0]) {
        snprintf(g_data.w_wind, sizeof(g_data.w_wind), "%s%s级", wdir, wscale);
    }

    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    g_data.w_upd_h = (uint8_t)tmv.tm_hour;
    g_data.w_upd_m = (uint8_t)tmv.tm_min;
    g_data.weather_valid = true;

    ESP_LOGI(TAG, "%s %s°C  %s  今日 %s~%s°C", g_data.w_text, g_data.w_temp,
             g_data.w_wind, g_data.w_low, g_data.w_high);
    return ESP_OK;
}
