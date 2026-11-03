#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "esp_err.h"

typedef struct httpd *httpd_handle_t;

typedef struct {
    size_t content_len;
    void *handle;
    int method;
    char *uri;
} httpd_req_t;

enum httpd_method_t { HTTP_GET = 1, HTTP_POST = 3 };

typedef esp_err_t (*httpd_uri_match_fn)(const char *registered, const char *request, size_t len);

typedef struct {
    const char *uri;
    int method;
    esp_err_t (*handler)(httpd_req_t *r);
    void *user_ctx;
} httpd_uri_t;

typedef struct {
    unsigned task_priority;
    size_t stack_size;
    int core_id;
    uint16_t server_port;
    uint16_t ctrl_port;
    uint16_t max_open_sockets;
    uint16_t max_uri_handlers;
    uint16_t max_resp_headers;
    uint16_t backlog_conn;
    bool lru_purge_enable;
    uint16_t recv_wait_timeout;
    uint16_t send_wait_timeout;
    bool uri_match_fn_wildcard;
} httpd_config_t;

#define HTTPD_DEFAULT_CONFIG() {0}
#define HTTPD_SOCK_ERR_TIMEOUT (-3)
#define HTTPD_RESP_USE_STRLEN  (-1)

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri_handler);
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type);
esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t buf_len);
esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *str);
esp_err_t httpd_resp_sendstr_chunk(httpd_req_t *r, const char *str);
int httpd_req_recv(httpd_req_t *r, char *buf, size_t buf_len);
