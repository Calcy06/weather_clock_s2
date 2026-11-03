#pragma once

#include <stddef.h>

#include "esp_err.h"

/*
 * 极简 HTTP GET 封装：一次把响应读进调用方缓冲（自动 NUL 结尾）。
 * 走 HTTPS 时用 IDF 内置证书包校验证书。
 */
esp_err_t http_get_text(const char *url, const char *user_agent, char *buf, size_t buflen);
