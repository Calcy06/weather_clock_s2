#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"

#include "cfg.h"
#include "form.h"
#include "prov.h"
#include "wifi.h"

static const char *TAG = "prov";

static httpd_handle_t s_server;
static wifi_ap_record_t s_aps[12];
static int s_ap_count;

/* 把用户输入拼进 HTML 属性前必须转义，否则引号会破坏页面结构 */
static void html_escape(char *dst, size_t dstlen, const char *src)
{
    size_t o = 0;
    for (const char *p = src; *p != '\0' && o + 7 < dstlen; p++) {
        const char *rep = NULL;
        switch (*p) {
        case '&': rep = "&amp;"; break;
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        case '"': rep = "&quot;"; break;
        case '\'': rep = "&#39;"; break;
        default: break;
        }
        if (rep != NULL) {
            size_t n = strlen(rep);
            memcpy(dst + o, rep, n);
            o += n;
        } else {
            dst[o++] = *p;
        }
    }
    dst[o] = '\0';
}

static void copy_field(const char *body, const char *key, char *dst, size_t dstlen)
{
    char tmp[200];
    if (form_get_str(body, key, tmp, sizeof(tmp))) {
        strncpy(dst, tmp, dstlen - 1);
        dst[dstlen - 1] = '\0';
    }
}

/* ------------------------------------------------------------------ 页面 */

static const char *HTML_HEAD =
    "<!DOCTYPE html><html lang=zh><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>天气时钟配网</title><style>"
    "body{font-family:-apple-system,'Segoe UI',sans-serif;margin:0;padding:16px 16px 44px;"
    "background:#0f1115;color:#e8eaed}"
    "h2{font-size:20px;margin:0 0 4px}p.sub{color:#7b8494;font-size:13px;margin:0 0 8px}"
    "label{display:block;margin:15px 0 6px;font-size:13px;color:#9aa4b2}"
    "input,select{width:100%;padding:11px;font-size:16px;border-radius:10px;"
    "border:1px solid #2a2f3a;background:#171a21;color:#e8eaed;box-sizing:border-box}"
    "button{width:100%;padding:15px;font-size:17px;margin-top:28px;border:0;"
    "border-radius:12px;background:#2d7ef7;color:#fff;font-weight:600}"
    ".row{display:flex;gap:10px}.row>*{flex:1}"
    ".tip{font-size:12px;color:#6b7380;margin-top:5px;line-height:1.5}"
    "hr{border:0;border-top:1px solid #232833;margin:22px 0 4px}"
    "</style></head><body>"
    "<h2>天气时钟 配网</h2>"
    "<p class=sub>ESP32-S2 天气时钟 · 保存后设备自动重启</p>"
    "<form method=post action=/save>";

static esp_err_t h_index(httpd_req_t *req)
{
    char buf[256];
    char esc[260];

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr_chunk(req, HTML_HEAD);

    httpd_resp_sendstr_chunk(req, "<label>WiFi 名称</label><select name=ssid>");
    for (int i = 0; i < s_ap_count; i++) {
        html_escape(esc, sizeof(esc), (const char *)s_aps[i].ssid);
        snprintf(buf, sizeof(buf), "<option value=\"%s\"%s>%s (%d)</option>", esc,
                 strcmp((const char *)s_aps[i].ssid, g_cfg.wifi_ssid) == 0 ? " selected" : "",
                 esc, s_aps[i].rssi);
        httpd_resp_sendstr_chunk(req, buf);
    }
    httpd_resp_sendstr_chunk(req, "</select>"
                                  "<div class=tip>列表里没有？直接在下面手动填（区分大小写）</div>"
                                  "<label>WiFi 名称（手动填写，可留空）</label>"
                                  "<input name=ssid_manual placeholder=例如 MyHome>");

    html_escape(esc, sizeof(esc), g_cfg.wifi_pass);
    snprintf(buf, sizeof(buf),
             "<label>WiFi 密码</label><input name=pass type=password value=\"%s\">", esc);
    httpd_resp_sendstr_chunk(req, buf);

    httpd_resp_sendstr_chunk(req, "<hr><label>心知天气私钥</label>");
    html_escape(esc, sizeof(esc), g_cfg.sen_key);
    snprintf(buf, sizeof(buf),
             "<input name=senkey value=\"%s\" placeholder=在 seniverse.com 免费注册获取>", esc);
    httpd_resp_sendstr_chunk(req, buf);
    html_escape(esc, sizeof(esc), g_cfg.sen_loc);
    snprintf(buf, sizeof(buf),
             "<label>城市代码</label><input name=senloc value=\"%s\" placeholder=beijing>"
             "<div class=tip>拼音或城市代码，如 beijing / shanghai / hangzhou</div>", esc);
    httpd_resp_sendstr_chunk(req, buf);

    httpd_resp_sendstr_chunk(req, "<hr><label>B站 UP 主 UID（可留空）</label>");
    html_escape(esc, sizeof(esc), g_cfg.bili_uid);
    snprintf(buf, sizeof(buf), "<input name=biliuid value=\"%s\" placeholder=103589285>", esc);
    httpd_resp_sendstr_chunk(req, buf);
    html_escape(esc, sizeof(esc), g_cfg.bili_bv);
    snprintf(buf, sizeof(buf),
             "<label>视频 BV 号（可留空）</label><input name=bilibv value=\"%s\" "
             "placeholder=BV1L7411c7jw>", esc);
    httpd_resp_sendstr_chunk(req, buf);

    httpd_resp_sendstr_chunk(req, "<hr><label>MQTT 服务器</label>");
    html_escape(esc, sizeof(esc), g_cfg.mqtt_uri);
    snprintf(buf, sizeof(buf), "<input name=mqtturi value=\"%s\">", esc);
    httpd_resp_sendstr_chunk(req, buf);

    snprintf(buf, sizeof(buf),
             "<label>屏幕亮度</label><input name=bright type=range min=20 max=255 value=%u>"
             "<div class=tip>当前 %u</div>",
             (unsigned)g_cfg.brightness, (unsigned)g_cfg.brightness);
    httpd_resp_sendstr_chunk(req, buf);

    snprintf(buf, sizeof(buf),
             "<label>页面轮播间隔（秒）</label><input name=pagesec type=number min=3 max=30 value=%u>",
             (unsigned)g_cfg.page_sec);
    httpd_resp_sendstr_chunk(req, buf);

    snprintf(buf, sizeof(buf),
             "<hr><label>闹钟时间</label><div class=row>"
             "<input name=alarm type=time value=\"%02u:%02u\">"
             "<select name=alarmen><option value=1%s>开启</option>"
             "<option value=0%s>关闭</option></select></div>"
             "<div class=tip>到点后有源蜂鸣器响 1 分钟，按板上 BOOT 键可立即停止</div>",
             (unsigned)g_cfg.alarm_hh, (unsigned)g_cfg.alarm_mm,
             g_cfg.alarm_en ? " selected" : "", g_cfg.alarm_en ? "" : " selected");
    httpd_resp_sendstr_chunk(req, buf);

    httpd_resp_sendstr_chunk(req, "<button type=submit>保存并重启</button></form></body></html>");
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t h_save(httpd_req_t *req)
{
    static char body[1024];
    int total = req->content_len;
    if (total >= (int)sizeof(body)) {
        total = sizeof(body) - 1;
    }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, body + got, total - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            return ESP_FAIL;
        }
        got += r;
    }
    body[got] = '\0';

    char manual[33];
    copy_field(body, "ssid_manual", manual, sizeof(manual));
    if (manual[0] != '\0') {
        strncpy(g_cfg.wifi_ssid, manual, sizeof(g_cfg.wifi_ssid) - 1);
    } else {
        copy_field(body, "ssid", g_cfg.wifi_ssid, sizeof(g_cfg.wifi_ssid));
    }

    copy_field(body, "pass", g_cfg.wifi_pass, sizeof(g_cfg.wifi_pass));
    copy_field(body, "senkey", g_cfg.sen_key, sizeof(g_cfg.sen_key));
    copy_field(body, "senloc", g_cfg.sen_loc, sizeof(g_cfg.sen_loc));
    copy_field(body, "biliuid", g_cfg.bili_uid, sizeof(g_cfg.bili_uid));
    copy_field(body, "bilibv", g_cfg.bili_bv, sizeof(g_cfg.bili_bv));
    copy_field(body, "mqtturi", g_cfg.mqtt_uri, sizeof(g_cfg.mqtt_uri));

    char v[32];
    if (form_get_str(body, "bright", v, sizeof(v))) {
        int b = atoi(v);
        g_cfg.brightness = (uint8_t)(b < 5 ? 5 : (b > 255 ? 255 : b));
    }
    if (form_get_str(body, "pagesec", v, sizeof(v))) {
        int s = atoi(v);
        g_cfg.page_sec = (uint8_t)(s < 3 ? 3 : (s > 30 ? 30 : s));
    }
    if (form_get_str(body, "alarm", v, sizeof(v))) {
        int hh = 0, mm = 0;
        if (sscanf(v, "%d:%d", &hh, &mm) == 2 && hh >= 0 && hh < 24 && mm >= 0 && mm < 60) {
            g_cfg.alarm_hh = (uint8_t)hh;
            g_cfg.alarm_mm = (uint8_t)mm;
        }
    }
    if (form_get_str(body, "alarmen", v, sizeof(v))) {
        g_cfg.alarm_en = (uint8_t)atoi(v);
    }

    cfg_save();
    ESP_LOGI(TAG, "配置已保存: ssid=%s city=%s alarm=%02u:%02u(%s)",
             g_cfg.wifi_ssid[0] ? g_cfg.wifi_ssid : "<空>", g_cfg.sen_loc,
             (unsigned)g_cfg.alarm_hh, (unsigned)g_cfg.alarm_mm,
             g_cfg.alarm_en ? "开" : "关");

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req,
                       "<meta charset=utf-8><body style='background:#0f1115;color:#e8eaed;"
                       "font-family:sans-serif;padding:40px 20px;text-align:center'>"
                       "<h2>已保存</h2><p>设备正在重启，请稍候…</p>");
    vTaskDelay(pdMS_TO_TICKS(900));
    esp_restart();
    return ESP_OK;
}

esp_err_t prov_start(void)
{
    esp_err_t err = wifi_start_ap();
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(400));          /* 等射频稳定再扫描 */

    s_ap_count = wifi_scan(s_aps, sizeof(s_aps) / sizeof(s_aps[0]));
    ESP_LOGI(TAG, "扫描到 %d 个 WiFi", s_ap_count);

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.max_uri_handlers = 4;
    hc.lru_purge_enable = true;
    err = httpd_start(&s_server, &hc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动 HTTP 服务失败: %s", esp_err_to_name(err));
        return err;
    }

    static const httpd_uri_t uri_index = {
        .uri = "/", .method = HTTP_GET, .handler = h_index, .user_ctx = NULL,
    };
    static const httpd_uri_t uri_save = {
        .uri = "/save", .method = HTTP_POST, .handler = h_save, .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &uri_index);
    httpd_register_uri_handler(s_server, &uri_save);
    return ESP_OK;
}

bool prov_running(void)
{
    return s_server != NULL;
}
