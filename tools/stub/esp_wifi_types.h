#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_event.h"
#include "esp_netif.h"

/* 事件基在真实 IDF 里由 ESP_EVENT_DEFINE_BASE 生成，桩用字符串代替 */
#define WIFI_EVENT "WIFI"
#define IP_EVENT "IP"

/* 只声明项目实际用到的部分 */

typedef enum {
    WIFI_AUTH_OPEN = 0,
    WIFI_AUTH_WEP,
    WIFI_AUTH_WPA_PSK,
    WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_WPA_WPA2_PSK,
} wifi_auth_mode_t;

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA,
} wifi_mode_t;

typedef enum {
    WIFI_PS_NONE,
    WIFI_PS_MIN_MODEM,
    WIFI_PS_MAX_MODEM,
} wifi_ps_type_t;

typedef enum {
    WIFI_IF_STA = 0,
    WIFI_IF_AP,
} wifi_interface_t;

typedef struct {
    uint8_t ssid[33];
    int8_t rssi;
    uint8_t bssid[6];
    uint8_t channel;
    wifi_auth_mode_t authmode;
} wifi_ap_record_t;

typedef struct {
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_scan_threshold_t;

typedef struct {
    uint8_t ssid[32];
    uint8_t password[64];
    wifi_scan_threshold_t threshold;
} wifi_sta_config_t;

typedef struct {
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint8_t channel;
    uint8_t max_connection;
    wifi_auth_mode_t authmode;
} wifi_ap_config_t;

typedef union {
    wifi_ap_config_t ap;
    wifi_sta_config_t sta;
} wifi_config_t;

typedef struct {
    bool show_hidden;
} wifi_scan_config_t;

typedef struct {
    int dummy;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() {0}

typedef enum {
    WIFI_EVENT_STA_START = 0,
    WIFI_EVENT_STA_STOP,
    WIFI_EVENT_STA_CONNECTED,
    WIFI_EVENT_STA_DISCONNECTED,
} wifi_event_t;

typedef struct {
    uint8_t reason;
} wifi_event_sta_disconnected_t;

typedef enum {
    IP_EVENT_STA_GOT_IP = 0,
} ip_event_t;

typedef struct {
    esp_netif_ip_info_t ip_info;
} ip_event_got_ip_t;
