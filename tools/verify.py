#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""主机侧一键自检（不需要 ESP-IDF、不需要板子）

用法:
    python tools/verify.py

做四件事：
  1. 用桩头文件对所有固件源码做语法检查（gcc -fsyntax-only）
  2. 编译并运行单元测试（表单解析 / UTF-8 / 字库 / 绘图边界）
  3. 编译并运行界面渲染，输出 docs/host_pages.png 供肉眼检查
  4. 把 docs/教学指南.md 里的完整示例代码抽出来编译，防止教程和实际 API 脱节
"""
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
CC = os.environ.get("CC", "gcc")

CFLAGS = [
    "-std=gnu11",
    "-Wall",
    "-Wextra",
    "-Wno-unused-parameter",
    "-Wno-address",
    "-I", os.path.join(ROOT, "main"),
    "-I", os.path.join(ROOT, "main", "fonts"),
    "-finput-charset=UTF-8",
    "-fexec-charset=UTF-8",
]

STUB_CFLAGS = CFLAGS + ["-I", os.path.join(ROOT, "tools", "stub")]
FONT_SRCS = [
    os.path.join(ROOT, "main", "fonts", "font.c"),
    os.path.join(ROOT, "main", "fonts", "font_ascii_8x16.c"),
    os.path.join(ROOT, "main", "fonts", "font_num_24x32.c"),
    os.path.join(ROOT, "main", "fonts", "font_cn_16x16.c"),
]

# MinGW 下 localtime_r/setenv 需要显式声明；Linux 下加这些也无害
POSIX_FLAGS = ["-D_POSIX_C_SOURCE=200809L"]

fails = []


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def step(title):
    print("\n\033[1m== %s ==\033[0m" % title)


def check_syntax():
    step("1/4 语法检查（桩头文件）")
    # 只在这些信号上判定失败，MinGW 的 POSIX 声明差异不算
    ignore = ("localtime_r", "setenv")
    bad = 0
    for path in sorted(glob.glob(os.path.join(ROOT, "main", "**", "*.c"), recursive=True)):
        r = run([CC, "-fsyntax-only"] + STUB_CFLAGS + POSIX_FLAGS + [path])
        lines = [
            ln for ln in r.stderr.splitlines()
            if ("error" in ln) or ("warning" in ln and not any(k in ln for k in ignore))
        ]
        if lines:
            bad += 1
            print("  \033[31mFAIL\033[0m %s" % os.path.relpath(path, ROOT))
            for ln in lines[:6]:
                print("       " + ln)
        else:
            print("  ok   %s" % os.path.relpath(path, ROOT))
    if bad:
        fails.append("语法检查 %d 个文件有问题" % bad)


def build_and_run(name, srcs, out):
    exe = os.path.join(BUILD, out)
    r = run([CC, "-O1"] + CFLAGS + srcs + ["-o", exe])
    if r.returncode != 0:
        print("  \033[31m编译失败\033[0m")
        print(r.stderr[:2000])
        fails.append(name + " 编译失败")
        return None
    return run([exe], cwd=ROOT)


def check_units():
    step("2/4 单元测试")
    srcs = [
        os.path.join(ROOT, "main", "form.c"),
        os.path.join(ROOT, "main", "gfx.c"),
        *FONT_SRCS,
        os.path.join(ROOT, "tools", "test_units.c"),
    ]
    r = build_and_run("单元测试", srcs, "test_units")
    if r is None:
        return
    print(r.stdout.rstrip())
    if r.returncode != 0:
        fails.append("单元测试未通过")


def check_render():
    step("3/4 界面渲染")
    srcs = [
        os.path.join(ROOT, "main", "gfx.c"),
        *FONT_SRCS,
        os.path.join(ROOT, "main", "ui.c"),
        os.path.join(ROOT, "tools", "host_test.c"),
    ]
    r = build_and_run("界面渲染", srcs, "host_test")
    if r is None:
        return
    print(r.stdout.rstrip())
    try:
        from PIL import Image
    except ImportError:
        print("  （未安装 Pillow，跳过拼图）")
        return
    docs = os.path.join(ROOT, "docs")
    names = ["clock", "weather", "bili", "msg"]
    ims = []
    for n in names:
        p = os.path.join(docs, "host_%s.pgm" % n)
        if not os.path.exists(p):
            fails.append("缺少渲染输出 %s" % p)
            return
        ims.append(Image.open(p))
    W, H = ims[0].size
    sheet = Image.new("L", (W * 2 + 8, H * 2 + 8), 50)
    for i, im in enumerate(ims):
        sheet.paste(im, ((i % 2) * (W + 4) + 2, (i // 2) * (H + 4) + 2))
    path = os.path.join(docs, "host_pages.png")
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(path)
    print("  四页预览 -> %s" % os.path.relpath(path, ROOT))



# 只在这些信号上判定失败：桩头文件与真实 IDF 的差异不算错
DOC_IGNORE = (
    "ESP_ERROR_CHECK",   # 桩里的 esp_err.h 没定义这个宏
    "comparison with string literal",   # 桩用字符串当事件基
    "struct timeval",    # 桩里的 esp_sntp.h 没拉 sys/time.h
    "localtime_r",
    "setenv",
)


def check_docs_code():
    """把教学指南里的"完整示例"抽出来编译一遍。

    教程一旦和真实 API 脱节，照抄的人就会踩坑，而且他不会知道是谁的错。
    这里用同一套桩头文件编译，能挡住 90% 的手误（参数顺序、字段名、忘 include）。
    """
    step("4/4 教程示例代码编译")
    import re

    doc = os.path.join(ROOT, "docs", "教学指南.md")
    if not os.path.exists(doc):
        print("  （没有 docs/教学指南.md，跳过）")
        return
    text = open(doc, encoding="utf-8").read()

    # 只挑"完整程序"级别的代码块：含 app_main 或完整函数定义的那种
    blocks = re.findall(r"```c\n(.*?)```", text, re.S)
    picked = [
        b for b in blocks
        if ("void app_main" in b
            or "static void on_mqtt" in b
            or "void prov_start" in b
            or ("bool weather_fetch" in b and "cJSON_Parse" in b))
    ]
    if not picked:
        print("  （没找到完整示例，跳过）")
        return

    bad = 0
    for i, code in enumerate(picked):
        path = os.path.join(BUILD, "doc_block_%d.c" % i)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(code)
        r = run([CC, "-fsyntax-only"] + STUB_CFLAGS + POSIX_FLAGS + [path])
        lines = [
            ln for ln in r.stderr.splitlines()
            if ("error" in ln) or ("warning" in ln and not any(k in ln for k in DOC_IGNORE))
        ]
        if lines:
            bad += 1
            print("  \033[31mFAIL\033[0m 代码块 #%d" % i)
            for ln in lines[:6]:
                print("       " + ln)
        else:
            print("  ok   代码块 #%d (%d 行)" % (i, code.count("\n")))
    if bad:
        fails.append("教学指南里 %d 段示例代码编译不过" % bad)


def main():
    if shutil.which(CC) is None:
        print("找不到 C 编译器：%s（请安装 gcc）" % CC)
        return 2
    os.makedirs(BUILD, exist_ok=True)

    check_syntax()
    check_units()
    check_render()
    check_docs_code()

    print()
    if fails:
        print("\033[31m自检未通过:\033[0m")
        for f in fails:
            print("  - " + f)
        return 1
    print("\033[32m全部自检通过\033[0m")
    return 0


if __name__ == "__main__":
    sys.exit(main())
