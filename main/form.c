#include <string.h>

#include "form.h"

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void url_decode(char *dst, const char *src, size_t dstlen)
{
    if (dst == NULL || dstlen == 0) {
        return;
    }
    size_t o = 0;
    while (*src != '\0' && o + 1 < dstlen) {
        if (*src == '+') {
            dst[o++] = ' ';
            src++;
        } else if (*src == '%' && hex_val(src[1]) >= 0 && hex_val(src[2]) >= 0) {
            dst[o++] = (char)(hex_val(src[1]) * 16 + hex_val(src[2]));
            src += 3;
        } else {
            dst[o++] = *src++;
        }
    }
    dst[o] = '\0';
}

bool form_get_str(const char *body, const char *key, char *out, size_t outlen)
{
    if (body == NULL || key == NULL || out == NULL || outlen == 0) {
        return false;
    }
    size_t klen = strlen(key);
    const char *p = body;
    while (*p != '\0') {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *vs = p + klen + 1;
            const char *ve = strchr(vs, '&');
            size_t n = (ve != NULL) ? (size_t)(ve - vs) : strlen(vs);
            char tmp[256];
            if (n >= sizeof(tmp)) {
                n = sizeof(tmp) - 1;
            }
            memcpy(tmp, vs, n);
            tmp[n] = '\0';
            url_decode(out, tmp, outlen);
            return true;
        }
        p = strchr(p, '&');
        if (p == NULL) {
            break;
        }
        p++;
    }
    return false;
}

void str_utf8_trim_tail(char *s)
{
    if (s == NULL) {
        return;
    }
    size_t n = strlen(s);
    if (n == 0) {
        return;
    }
    /* 从结尾向前找序列起始字节（10xxxxxx 是续字节） */
    size_t i = n;
    while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) {
        i--;
    }
    if (i == 0) {
        s[0] = '\0';          /* 全是续字节，整个清掉 */
        return;
    }
    /* s[i-1] 是起始字节，看它声明的序列长度 */
    unsigned char lead = (unsigned char)s[i - 1];
    int seqlen = 1;
    if (lead >= 0xF0) {
        seqlen = 4;
    } else if (lead >= 0xE0) {
        seqlen = 3;
    } else if (lead >= 0xC0) {
        seqlen = 2;
    }
    /* 序列不完整（被截断）就把整个残缺序列删掉 */
    if (i - 1 + seqlen > n) {
        s[i - 1] = '\0';
    }
}
