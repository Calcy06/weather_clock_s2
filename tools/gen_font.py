#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""点阵字库生成器 —— 天气时钟 (ESP32-S2 / SSD1306 128x64)

把系统字体渲染成 1-bit 点阵，输出 C 数组供 SSD1306 直接绘制。

  font_ascii_8x16 : ASCII 32..126 + 常用符号    consola 14   格子 8x16   宽 8px
  font_num_24x32  : 0-9 : - . 空格             consolab 40  格子 24x32  宽 24px
  font_cn_16x16   : GB2312 符号区 + 一级汉字    simhei 16    格子 16x16  宽 16px

字符表按 Unicode 码点升序输出，固件里用二分查找取字形。

用法:
  python tools/gen_font.py            # 仅生成 C 文件
  python tools/gen_font.py --preview  # 生成 C 文件 + 四页界面预览 + 布局校验
"""
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT_DIR = os.path.join(ROOT, "main", "fonts")
DOC_DIR = os.path.join(ROOT, "docs")

FONT_CANDIDATES = {
    "mono":  ["consola.ttf", "DejaVuSansMono.ttf", "LiberationMono-Regular.ttf"],
    "monob": ["consolab.ttf", "DejaVuSansMono-Bold.ttf", "LiberationMono-Bold.ttf"],
    "cn":    ["simhei.ttf", "msyh.ttc", "NotoSansCJK-Regular.ttc", "wqy-microhei.ttc"],
}

# ASCII 字体额外收录的符号（度、摄氏度、正负、间隔号、乘号、箭头）
EXTRA_SYMBOLS = "\u00b0\u2103\u00b1\u00b7\u00d7\u2192\u2190\u2191\u2193"
ASCII_CHARS = "".join(chr(c) for c in range(32, 127)) + EXTRA_SYMBOLS
NUM_CHARS = "0123456789:-. "


def find_font(kind):
    local = os.environ.get("WINFONT", "C:/Windows/Fonts")
    for name in FONT_CANDIDATES[kind]:
        candidates = [os.path.join(local, name)]
        for base in ("/usr/share/fonts", "/usr/local/share/fonts"):
            for dirpath, _, files in os.walk(base):
                if name in files:
                    candidates.append(os.path.join(dirpath, name))
        for p in candidates:
            if os.path.exists(p):
                return p
    raise SystemExit("[错误] 找不到字体 %s，可设置环境变量 WINFONT 指向字体目录" % kind)


def gb2312_chars(first=0xA1, last=0xD7):
    """按 GB2312 区段取字符：0xA1-0xA9 符号区，0xB0-0xD7 一级常用汉字"""
    out = []
    for hi in range(first, last + 1):
        for lo in range(0xA1, 0xFF):
            try:
                out.append(bytes([hi, lo]).decode("gb2312"))
            except UnicodeDecodeError:
                continue
    return "".join(out)


def dedup(s):
    seen, out = set(), []
    for c in s:
        if c not in seen:
            seen.add(c)
            out.append(c)
    return "".join(out)


def render_glyph(ch, font_path, size, w, h, ref_ch, valign="top"):
    """渲染单字符成 w*h 的 1-bit 位图。

    所有字符共用同一裁剪窗口（由 ref_ch 决定），保证基线一致：
      top    参考墨迹顶端贴格子顶部（汉字）
      center 参考墨迹垂直居中（ASCII 符号，与汉字中心对齐）
      bottom 参考墨迹底端贴格子底部（大号数字）
    """
    font = ImageFont.truetype(font_path, size)
    rx0, ry0, rx1, ry1 = font.getbbox(ref_ch)
    if valign == "top":
        off_y = ry0
    elif valign == "bottom":
        off_y = ry1 - h
    else:                       # center：与 16x16 汉字中心对齐
        off_y = ry0 - (h - (ry1 - ry0)) // 2
    canvas = Image.new("L", (w * 4, h * 4), 0)
    ImageDraw.Draw(canvas).text((w - rx0, h - off_y), ch, font=font, fill=255)
    crop = canvas.crop((w, h, w + w, h + h))
    return crop.point(lambda p: 255 if p >= 96 else 0).convert("1")


def to_bytes(img, w, h):
    bpr = (w + 7) // 8
    out = bytearray()
    px = img.load()
    for y in range(h):
        for bx in range(bpr):
            v = 0
            for b in range(8):
                x = bx * 8 + b
                if x < w and px[x, y]:
                    v |= 0x80 >> b
            out.append(v)
    return bytes(out)


def build(chars, kind, size, w, h, ref_ch, valign):
    chars = "".join(sorted(dedup(chars), key=ord))   # 升序排列便于二分查找
    path = find_font(kind)
    glyphs = [to_bytes(render_glyph(c, path, size, w, h, ref_ch, valign), w, h) for c in chars]
    return chars, glyphs


def c_bytes(name, data, width=16):
    lines = ["static const uint8_t %s[] = {" % name]
    for i in range(0, len(data), width):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + width]) + ",")
    lines.append("};")
    return "\n".join(lines)


def c_u16(name, values, width=24):
    lines = ["static const uint16_t %s[] = {" % name]
    for i in range(0, len(values), width):
        lines.append("    " + ", ".join("%d" % v for v in values[i:i + width]) + ",")
    lines.append("};")
    return "\n".join(lines)


def write_font(fname, var, chars, glyphs, w, h, comment):
    bpr = (w + 7) // 8
    flat = b"".join(glyphs)
    cps = [ord(c) for c in chars]
    esc = chars.replace("\\", "\\\\").replace('"', '\\"')
    cpath = os.path.join(OUT_DIR, fname + ".c")
    with open(cpath, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* %s —— 由 tools/gen_font.py 生成，请勿手改 */\n" % comment)
        f.write('#include "%s.h"\n\n' % fname)
        f.write("/* 字符表（UTF-8，按码点升序） */\n")
        f.write('static const char %s_chars[] = "%s";\n\n' % (var, esc))
        f.write("/* 码点表（二分查找用） */\n")
        f.write(c_u16(var + "_cps", cps) + "\n\n")
        f.write("/* 点阵数据：%d 字形 x %d 字节 */\n" % (len(glyphs), bpr * h))
        f.write(c_bytes(var + "_data", flat) + "\n\n")
        f.write("const ui_font_t %s = {\n" % var)
        f.write("    .data = %s_data,\n" % var)
        f.write("    .chars = %s_chars,\n" % var)
        f.write("    .cps = %s_cps,\n" % var)
        f.write("    .w = %d, .h = %d, .bpr = %d,\n" % (w, h, bpr))
        f.write("    .count = %d,\n};\n" % len(glyphs))
    hpath = os.path.join(OUT_DIR, fname + ".h")
    with open(hpath, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* %s —— 自动生成 */\n#pragma once\n\n#include \"font.h\"\n\nextern const ui_font_t %s;\n" % (comment, var))
    print("  %-18s %4d 字形  %6.1f KB" % (fname, len(glyphs), len(flat) / 1024.0))


# ------------------------------------------------------------------ 界面预览
def draw_preview(f8, n8, fn, nn, fc, nc):
    W, H = 128, 64
    i8 = {c: i for i, c in enumerate(f8)}
    inum = {c: i for i, c in enumerate(fn)}
    icn = {c: i for i, c in enumerate(fc)}
    problems = []

    def blit(scr, g, w, h, x, y):
        scr.paste(Image.frombytes("1", (w, h), bytes(g)), (x, y))

    def text(scr, s, x, y, maxw=None):
        """与 C 侧 ui_draw_text 一致：ASCII 8px / 汉字 16px / 缺字打 ?"""
        x0 = x
        for ch in s:
            if ch in i8:
                blit(scr, n8[i8[ch]], 8, 16, x, y)
                x += 8
            elif ch in icn:
                blit(scr, nc[icn[ch]], 16, 16, x, y)
                x += 16
            else:
                problems.append("缺字 '%s' (U+%04X)" % (ch, ord(ch)))
                blit(scr, n8[i8["?"]], 8, 16, x, y)
                x += 8
        if maxw is not None and x - x0 > maxw:
            problems.append("越界: \"%s\" 宽 %d > %d" % (s, x - x0, maxw))
        if x > W:
            problems.append("出屏: \"%s\" 右端 %d > %d" % (s, x, W))
        return x

    def num(scr, s, x, y):
        for ch in s:
            blit(scr, nn[inum[ch]], 24, 32, x, y)
            x += 24
        if x > W:
            problems.append("时间出屏: 右端 %d" % x)
        return x

    # Page 0 时钟主页
    p0 = Image.new("1", (W, H), 0)
    text(p0, "11月03日周一", 0, 0, 100)
    text(p0, "41", 108, 0, 20)
    num(p0, "09:41", 4, 16)
    text(p0, "晴 23\u00b0C", 2, 48, 70)
    text(p0, "湿45%", 74, 48, 54)

    # Page 1 天气详情（每行左项 + 右项，单项宽度需 <= 60px 才放得下）
    p1 = Image.new("1", (W, H), 0)
    text(p1, "北京 晴", 2, 0, 124)
    text(p1, "23\u00b0C", 2, 16, 36)
    text(p1, "体感 21\u00b0C", 40, 16, 86)
    text(p1, "湿度 45%", 2, 32, 64)
    text(p1, "东南3级", 68, 32, 58)
    text(p1, "空气 良", 2, 48, 56)
    text(p1, "19:20", 60, 48, 66)

    # Page 2 哔哩信息
    p2 = Image.new("1", (W, H), 0)
    text(p2, "B站 UP主", 2, 0, 124)
    text(p2, "粉丝 128000", 2, 16, 124)
    text(p2, "播放 3500000", 2, 32, 124)
    text(p2, "BV1L7411c7jw", 2, 48, 124)

    # Page 3 MQTT 消息
    p3 = Image.new("1", (W, H), 0)
    text(p3, "消息", 2, 0, 32)
    text(p3, "19:20", 88, 0, 40)
    text(p3, "收到一条消息", 2, 16, 124)
    text(p3, "明早7点带伞", 2, 32, 124)
    text(p3, "闹钟06:30", 2, 48, 124)

    os.makedirs(DOC_DIR, exist_ok=True)
    sheet = Image.new("L", (W * 2 + 8, H * 2 + 8), 50)
    for i, scr in enumerate((p0, p1, p2, p3)):
        sheet.paste(scr.convert("L"), ((i % 2) * (W + 4) + 2, (i // 2) * (H + 4) + 2))
    path = os.path.join(DOC_DIR, "preview_pages.png")
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(path)
    print("  预览图 ->", path)
    if problems:
        print("  [布局问题 %d]" % len(problems))
        for p in problems:
            print("     -", p)
    else:
        print("  布局校验通过：无缺字、无越界")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print("渲染字库:")
    f8, g8 = build(ASCII_CHARS, "mono", 14, 8, 16, "H", "center")
    write_font("font_ascii_8x16", "font_ascii_8x16", f8, g8, 8, 16, "ASCII + 符号 8x16")

    fn, gn = build(NUM_CHARS, "monob", 40, 24, 32, "0", "bottom")
    write_font("font_num_24x32", "font_num_24x32", fn, gn, 24, 32, "大号数字 24x32")

    fc, gc = build(gb2312_chars(), "cn", 16, 16, 16, "国", "top")
    write_font("font_cn_16x16", "font_cn_16x16", fc, gc, 16, 16, "GB2312 汉字 16x16")

    if "--preview" in sys.argv:
        draw_preview(f8, g8, fn, gn, fc, gc)


if __name__ == "__main__":
    main()
