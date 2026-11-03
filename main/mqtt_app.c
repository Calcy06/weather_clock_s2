#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "mqtt_client.h"

#include "app_cfg.h"
#include "app_data.h"
#include "form.h"
#include "buzzer.h"
#include "cfg.h"
#include "mqtt_app.h"
#include "wifi.h"

static const char *TAG = "mqtt";

volatile int g_req_page = -1;   /* 手机下发的页面切换请求，-1 表示无 */

static char s_ctrl[80];
static char s_chat[80];
static char s_report[80];
static char s_client_id[32];
static esp_mqtt_client_handle_t s_client;
static bool s_connected;

const char *mqtt_topic_control(void)
{
    return s_ctrl;
}

const char *mqtt_topic_chat(void)
{
    return s_chat;
}

const char *mqtt_topic_report(void)
{
    return s_report;
}

/* 最近 3 条消息，新的在最前面 */
static void push_message(const char *text, size_t len)
{
    char buf[28];
    size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, text, n);
    buf[n] = '\0';
    str_utf8_trim_tail(buf);      /* 截断可能切在汉字中间，修掉残缺尾部 */
    if (buf[0] == '\0') {
        return;
    }

    memmove(g_data.msg[1], g_data.msg[0], sizeof(g_data.msg[0]) * 2);
    strncpy(g_data.msg[0], buf, sizeof(g_data.msg[0]) - 1);
    g_data.msg[0][sizeof(g_data.msg[0]) - 1] = '\0';
    if (g_data.msg_count < 3) {
        g_data.msg_count++;
    }

    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    g_data.msg_upd_h = (uint8_t)tmv.tm_hour;
    g_data.msg_upd_m = (uint8_t)tmv.tm_min;
    g_data.msg_serial++;          /* 渲染任务检测到变化就弹屏 */
    ESP_LOGI(TAG, "收到消息: %s", buf);
}

void mqtt_report(void)
{
    if (!s_connected || s_client == NULL) {
        return;
    }
    char payload[220];
    snprintf(payload, sizeof(payload),
             "{\"wifi\":\"%s\",\"heap\":%u,\"weather\":\"%s %sC\",\"alarm\":\"%02u:%02u\","
             "\"en\":%u,\"ring\":%u}",
             wifi_sta_ip(), (unsigned)esp_get_free_heap_size(), g_data.w_text, g_data.w_temp,
             (unsigned)g_cfg.alarm_hh, (unsigned)g_cfg.alarm_mm, (unsigned)g_cfg.alarm_en,
             alarm_ringing() ? 1u : 0u);
    esp_mqtt_client_publish(s_client, s_report, payload, 0, 0, 0);
}

static void handle_control(const char *cmd)
{
    if (strncmp(cmd, "msg ", 4) == 0) {
        push_message(cmd + 4, strlen(cmd + 4));
    } else if (strcmp(cmd, "hb") == 0) {
        mqtt_report();
    } else if (strncmp(cmd, "buz ", 4) == 0) {
        buzzer_set(atoi(cmd + 4) != 0);
    } else if (strncmp(cmd, "page ", 5) == 0) {
        int p = atoi(cmd + 5);
        if (p >= 0 && p < 4) {
            g_req_page = p;
        }
    } else if (strncmp(cmd, "alarm ", 6) == 0) {
        int hh = 0, mm = 0;
        char st[8] = {0};
        if (sscanf(cmd + 6, "%d:%d %7s", &hh, &mm, st) == 2 && hh >= 0 && hh < 24 && mm >= 0 &&
            mm < 60) {
            g_cfg.alarm_hh = (uint8_t)hh;
            g_cfg.alarm_mm = (uint8_t)mm;
            g_cfg.alarm_en = 1;
            cfg_save();
            mqtt_report();
        }
    } else if (strcmp(cmd, "stop") == 0) {
        alarm_stop();
    } else {
        ESP_LOGW(TAG, "未知指令: %s", cmd);
    }
}

static void on_mqtt_event(void *args, esp_event_base_t base, int32_t id, void *data)
{
    (void)args;
    (void)base;
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)data;

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "已连接 %s", g_cfg.mqtt_uri);
        esp_mqtt_client_subscribe(s_client, s_ctrl, 0);
        esp_mqtt_client_subscribe(s_client, s_chat, 0);
        ESP_LOGI(TAG, "控制主题: %s", s_ctrl);
        ESP_LOGI(TAG, "聊天主题: %s", s_chat);
        ESP_LOGI(TAG, "上报主题: %s", s_report);
        mqtt_report();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "连接断开，等待自动重连");
        break;
    case MQTT_EVENT_DATA: {
        /* topic/data 都不是以 NUL 结尾，必须按长度处理 */
        char topic[80];
        size_t tn = (size_t)e->topic_len < sizeof(topic) - 1 ? (size_t)e->topic_len
                                                             : sizeof(topic) - 1;
        memcpy(topic, e->topic, tn);
        topic[tn] = '\0';

        char payload[160];
        size_t pn = (size_t)e->data_len < sizeof(payload) - 1 ? (size_t)e->data_len
                                                             : sizeof(payload) - 1;
        memcpy(payload, e->data, pn);
        payload[pn] = '\0';

        if (strcmp(topic, s_chat) == 0) {
            push_message(payload, pn);
        } else if (strcmp(topic, s_ctrl) == 0) {
            handle_control(payload);
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT 错误，将自动重连");
        break;
    default:
        break;
    }
}

esp_err_t mqtt_start(void)
{
    if (g_cfg.mqtt_uri[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_client_id, sizeof(s_client_id), "wc-%02X%02X%02X", mac[3], mac[4], mac[5]);
    snprintf(s_ctrl, sizeof(s_ctrl), "wc/%02X%02X%02X/control", mac[3], mac[4], mac[5]);
    snprintf(s_chat, sizeof(s_chat), "wc/%02X%02X%02X/chat", mac[3], mac[4], mac[5]);
    snprintf(s_report, sizeof(s_report), "wc/%02X%02X%02X/report", mac[3], mac[4], mac[5]);

    esp_mqtt_client_config_t cfg = {0};
    cfg.broker.address.uri = g_cfg.mqtt_uri;
    cfg.credentials.client_id = s_client_id;
    if (g_cfg.mqtt_user[0] != '\0') {
        cfg.credentials.username = g_cfg.mqtt_user;
    }
    if (g_cfg.mqtt_pass[0] != '\0') {
        cfg.credentials.authentication.password = g_cfg.mqtt_pass;
    }
    cfg.session.keepalive = 30;
    cfg.network.reconnect_timeout_ms = 5000;
    cfg.network.disable_auto_reconnect = false;

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        return ESP_FAIL;
    }
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_mqtt_event, NULL);
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动 MQTT 失败: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "MQTT 客户端已启动: %s", g_cfg.mqtt_uri);
    return ESP_OK;
}

bool mqtt_connected(void)
{
    return s_connected;
}
