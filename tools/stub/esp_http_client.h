#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct esp_http_client *esp_http_client_handle_t;

typedef esp_err_t (*http_event_handle_cb)(void *evt);

typedef struct {
    const char *url;
    int timeout_ms;
    const void *crt_bundle_attach;
    bool disable_auto_redirect;
    bool keep_alive_enable;
    const char *method;
    const char *host;
    int port;
    const char *path;
} esp_http_client_config_t;

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key,
                                     const char *value);
esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len);
esp_err_t esp_http_client_close(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
