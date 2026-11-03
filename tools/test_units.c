/*
 * 主机单元测试：表单解析、UTF-8、字库二分查找、绘图边界。
 * 用 PC gcc 编译运行，不参与固件。
 */
#include <stdio.h>
#include <string.h>

#include "font.h"
#include "form.h"
#include "gfx.h"

static int g_fail;

#define CHECK(cond, fmt, ...)                                  \
    do {                                                       \
        if (!(cond)) {                                         \
            printf("  [FAIL] " fmt "\n", ##__VA_ARGS__);       \
            g_fail++;                                          \
        }                                                      \
    } while (0)

static void test_url_decode(void)
{
    printf("[url_decode]\n");
    char out[64];

    url_decode(out, "beijing", sizeof(out));
    CHECK(strcmp(out, "beijing") == 0, "普通字符串: %s", out);

    url_decode(out, "%E5%8C%97%E4%BA%AC", sizeof(out));
    CHECK(strcmp(out, "北京") == 0, "中文百分号解码: %s", out);

    url_decode(out, "my+home", sizeof(out));
    CHECK(strcmp(out, "my home") == 0, "加号转空格: %s", out);

    url_decode(out, "mqtt%3A%2F%2Fhost", sizeof(out));
    CHECK(strcmp(out, "mqtt://host") == 0, "MQTT URI 解码: %s", out);

    url_decode(out, "bad%zz", sizeof(out));
    CHECK(strcmp(out, "bad%zz") == 0, "非法百分号应原样保留: %s", out);

    url_decode(out, "", sizeof(out));
    CHECK(out[0] == '\0', "空串");

    /* 缓冲区截断：不能溢出 */
    char small[4];
    url_decode(small, "abcdefgh", sizeof(small));
    CHECK(strlen(small) == 3, "截断长度应为 3，实际 %u", (unsigned)strlen(small));
}

static void test_form_get(void)
{
    printf("[form_get_str]\n");
    char out[64];
    const char *body = "ssid_manual=&ssid=MyHome&pass=p%40ss+word&alarm=07%3A30&alarmen=1";

    CHECK(form_get_str(body, "ssid", out, sizeof(out)), "取 ssid 应成功");
    CHECK(strcmp(out, "MyHome") == 0, "ssid 不应被 ssid_manual 干扰，实际 %s", out);

    CHECK(form_get_str(body, "ssid_manual", out, sizeof(out)), "取 ssid_manual 应成功");
    CHECK(out[0] == '\0', "空值应为空串");

    CHECK(form_get_str(body, "pass", out, sizeof(out)), "取 pass");
    CHECK(strcmp(out, "p@ss word") == 0, "密码解码: %s", out);

    CHECK(form_get_str(body, "alarm", out, sizeof(out)), "取 alarm");
    CHECK(strcmp(out, "07:30") == 0, "时间解码: %s", out);

    CHECK(!form_get_str(body, "notexist", out, sizeof(out)), "不存在的键应返回 false");

    /* 最后一个字段没有 & 结尾 */
    CHECK(form_get_str("a=1&b=2", "b", out, sizeof(out)), "末字段应能取到");
    CHECK(strcmp(out, "2") == 0, "末字段值: %s", out);
}

static void test_utf8(void)
{
    printf("[utf8]\n");
    struct {
        const char *s;
        unsigned int cp;
    } cases[] = {
        {"A", 0x41},
        {"晴", 0x6674},
        {"°", 0x00B0},
        {"，", 0xFF0C},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *p = cases[i].s;
        unsigned int cp = ui_utf8_next(&p);
        CHECK(cp == cases[i].cp, "「%s」解码为 U+%04X，期望 U+%04X", cases[i].s, cp,
              cases[i].cp);
        CHECK(*p == '\0', "「%s」解码后应指向结尾", cases[i].s);
    }

    const char *bad = "\x80";
    unsigned int cp = ui_utf8_next(&bad);
    CHECK(cp == '?', "非法首字节应返回 '?'，实际 U+%04X", cp);
}

static void test_utf8_trim(void)
{
    printf("[str_utf8_trim_tail]\n");
    char buf[32];

    /* 完整字符串不应被改动 */
    strcpy(buf, "\xE4\xB8\xAD\xE6\x96\x87ok");
    str_utf8_trim_tail(buf);
    CHECK(strcmp(buf, "\xE4\xB8\xAD\xE6\x96\x87ok") == 0, "完整串不应改动: %s", buf);

    /* 3 字节汉字被切剩 2 字节 → 删掉残缺 */
    strcpy(buf, "abc\xE4\xB8");
    str_utf8_trim_tail(buf);
    CHECK(strcmp(buf, "abc") == 0, "残缺 3 字节汉字应删尾: '%s'", buf);

    /* 3 字节汉字被切剩 1 字节 → 删掉残缺 */
    strcpy(buf, "abc\xE4");
    str_utf8_trim_tail(buf);
    CHECK(strcmp(buf, "abc") == 0, "残缺 1 字节应删尾: '%s'", buf);

    /* 纯 ASCII 不动 */
    strcpy(buf, "hello");
    str_utf8_trim_tail(buf);
    CHECK(strcmp(buf, "hello") == 0, "ASCII 不应改动");

    /* 空串安全 */
    buf[0] = '\0';
    str_utf8_trim_tail(buf);
    CHECK(buf[0] == '\0', "空串安全");

    /* 全是续字节（极端垃圾）→ 清空 */
    strcpy(buf, "\x80\x80");
    str_utf8_trim_tail(buf);
    CHECK(buf[0] == '\0', "纯续字节应清空");
}

static void test_fonts(void)
{
    printf("[fonts]\n");
    const ui_font_t *alls[] = {&font_ascii_8x16, &font_num_24x32, &font_cn_16x16};

    for (unsigned k = 0; k < sizeof(alls) / sizeof(alls[0]); k++) {
        const ui_font_t *f = alls[k];
        size_t gsize = (size_t)f->bpr * f->h;
        CHECK(f->count > 0, "字体为空");
        for (int i = 0; i < f->count; i++) {
            /* 码点必须严格升序，否则二分查找会失效 */
            if (i > 0) {
                CHECK(f->cps[i] > f->cps[i - 1], "码点未升序: idx %d U+%04X 后跟 U+%04X", i,
                      f->cps[i - 1], f->cps[i]);
            }
            const uint8_t *g = ui_font_glyph(f, f->cps[i]);
            CHECK(g == f->data + (size_t)i * gsize, "字形指针错位 idx=%d", i);
        }
    }

    CHECK(ui_font_glyph(&font_ascii_8x16, 'A') != NULL, "ASCII 'A' 应存在");
    CHECK(ui_font_glyph(&font_ascii_8x16, 0x00B0) != NULL, "度符号应存在");
    CHECK(ui_font_glyph(&font_num_24x32, '7') != NULL, "大号数字 7 应存在");
    CHECK(ui_font_glyph(&font_num_24x32, ':') != NULL, "冒号应存在");
    CHECK(ui_font_glyph(&font_cn_16x16, 0x6674) != NULL, "汉字「晴」应存在");
    CHECK(ui_font_glyph(&font_cn_16x16, 0x4E91) != NULL, "汉字「云」应存在");
    CHECK(ui_font_glyph(&font_cn_16x16, 0x9F98) == NULL, "生僻字应查不到");
}

static void test_gfx(void)
{
    printf("[gfx]\n");
    gfx_clear();

    CHECK(gfx_text_width("晴") == 16, "汉字宽度");
    CHECK(gfx_text_width("AB") == 16, "两个 ASCII 宽度");
    CHECK(gfx_text_width("晴 A") == 32, "混排宽度 %d", gfx_text_width("晴 A"));
    CHECK(gfx_text_width("") == 0, "空串宽度");

    /* 越界绘制不能写到缓冲区外 */
    gfx_pixel(-5, -5, true);
    gfx_pixel(200, 200, true);
    gfx_hline(120, 63, 50, true);
    gfx_fill_rect(-10, -10, 200, 200, true);

    gfx_clear();
    gfx_text(0, 0, "晴");
    const uint8_t *fb = gfx_fb();
    /* 第 0 行前两个字节应该有笔画（「晴」左上角有内容） */
    int non_zero = 0;
    for (int i = 0; i < 32; i++) {
        if (fb[i] != 0) {
            non_zero++;
        }
    }
    CHECK(non_zero > 0, "汉字应写入帧缓冲");

    /* 文本绘制不应超出右边界 */
    int w = gfx_text_max(120, 0, 40, "晴晴晴");
    CHECK(120 + w <= 128, "截断后宽度 %d 超出屏幕", w);
}

int main(void)
{
    test_url_decode();
    test_form_get();
    test_utf8();
    test_utf8_trim();
    test_fonts();
    test_gfx();

    if (g_fail == 0) {
        printf("\n全部测试通过\n");
        return 0;
    }
    printf("\n%d 项失败\n", g_fail);
    return 1;
}
