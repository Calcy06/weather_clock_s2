#pragma once

#include <stdbool.h>
#include <stddef.h>

/* URL 解码：%XX 与 '+' → 空格，写入 dst（保证 NUL 结尾） */
void url_decode(char *dst, const char *src, size_t dstlen);

/*
 * 从 application/x-www-form-urlencoded 取字段值。
 * 精确匹配 key（"ssid" 不会误命中 "ssid_manual"），找不到返回 false。
 */
bool form_get_str(const char *body, const char *key, char *out, size_t outlen);

/*
 * UTF-8 尾部修剪：把结尾被切断的半个汉字（不完整的多字节序列）删掉。
 * 截断任意长度文本后必须调用，否则屏幕上会出现乱码字形。
 */
void str_utf8_trim_tail(char *s);
