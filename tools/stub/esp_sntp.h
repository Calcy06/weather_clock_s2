#pragma once

#include <stdbool.h>

#include <sys/time.h>

typedef enum {
    ESP_SNTP_OPMODE_POLL,
    ESP_SNTP_OPMODE_LISTENONLY,
} esp_sntp_operatingmode_t;

void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t mode);
void esp_sntp_setservername(uint8_t idx, const char *server);
void esp_sntp_init(void);
void esp_sntp_stop(void);

/* 对时成功回调（真实 IDF 里参数是 struct timeval *） */
void esp_sntp_set_time_sync_notification_cb(void (*cb)(struct timeval *tv));
