#include <stdio.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "net_http.h"

static const char *TAG = "http";

esp_err_t http_get_text(const char *url, const char *user_agent, char *buf, size_t buflen)
{
    if (url == NULL || buf == NULL || buflen < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (c == NULL) {
        return ESP_FAIL;
    }
    if (user_agent != NULL) {
        esp_http_client_set_header(c, "User-Agent", user_agent);
    }

    esp_err_t err = esp_http_client_open(c, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "连接失败 %s: %s", url, esp_err_to_name(err));
        esp_http_client_cleanup(c);
        return err;
    }

    esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);
    size_t total = 0;
    while (total + 1 < buflen) {
        int r = esp_http_client_read(c, buf + total, buflen - 1 - total);
        if (r <= 0) {
            break;
        }
        total += (size_t)r;
    }
    buf[total] = '\0';

    esp_http_client_close(c);
    esp_http_client_cleanup(c);

    if (status != 200) {
        ESP_LOGW(TAG, "HTTP %d %s", status, url);
        return ESP_FAIL;
    }
    if (total == 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
