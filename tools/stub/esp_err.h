/* 仅用于主机语法检查的 ESP-IDF 桩头，签名对齐 IDF v6.0 文档 */
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NVS_NO_FREE_PAGES 0x110D
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1110

#define ESP_ERR_INVALID_RESPONSE 0x108

const char *esp_err_to_name(esp_err_t code);
