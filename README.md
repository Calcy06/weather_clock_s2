# 物联网天气时钟 · ESP32‑S2 版

把太极创客的 **ESP8266 + TFT 彩屏天气时钟**（[原教程](http://www.taichi-maker.com/homepage/esp8266-nodemcu-iot/iot-tuttorial/mqtt-application/1-1-3-iot-weather-clock-guide/)）迁移到
**ESP32‑S2‑DevKitC‑1‑N8R2 + SSD1306 单色 OLED + 有源蜂鸣器 + ESP‑IDF v6.x**。

原项目是 Arduino 框架 + TFT_eSPI 彩屏 + SPIFFS 图片轮播；本项目是 **纯 ESP‑IDF（C）+ 单色 OLED + 多页信息轮播**，
功能对齐、实现方式完全不同，没有一行代码可以直接照搬。

---

## 1. 功能对照

| 原项目（ESP8266 / Arduino） | 本项目（ESP32‑S2 / ESP‑IDF） | 说明 |
|---|---|---|
| NTP 网络对时 | ✅ SNTP + POSIX 时区 | 支持任意时区（配网页可改） |
| 心知天气 实时天气 | ✅ now.json + daily.json | 额外拿到今日温度区间、风向风力（免费版无湿度权限）|
| 哔哩哔哩 UP 主粉丝 / 视频播放量 | ✅ B站公开接口 | 需自带浏览器 UA，否则 412 |
| TFT 彩屏 + SPIFFS 背景图轮播 | ⚠️ 改为 4 页信息轮播 | OLED 单色 128×64，放不下彩图 |
| WiFiManager 网页配网 | ✅ 自建 SoftAP + HTTP 配网页 | 配网页还能配心知 KEY、闹钟、亮度 |
| 然也物联 MQTT 手机控制 | ✅ 标准 MQTT 三主题 | 任意 MQTT 客户端都能控（MQTTX / 小程序） |
| 然也物联手机 App | ❌ | 那个 App 是绑定原项目主题格式的，改用通用客户端 |
| —（原项目没有） | ✅ 有源蜂鸣器闹钟 | 到点响 1 分钟，BOOT 键可停 |

---

## 2. 硬件与接线

| 元件 | 型号 | 备注 |
|---|---|---|
| 主控 | ESP32‑S2‑DevKitC‑1‑N8R2 | 8MB Flash / 2MB PSRAM |
| 屏 | SSD1306 128×64 I2C 四针 | 地址 0x3C（少数是 0x3D，代码会自动探测） |
| 蜂鸣器 | 有源、高电平触发 | 3.3V 版本可直接驱动 |

### 接线表

| SSD1306 | ESP32‑S2 |
|---|---|
| VCC | **3V3**（不要接 5V） |
| GND | GND |
| SDA | **GPIO8** |
| SCL | **GPIO9** |

| 蜂鸣器 | ESP32‑S2 |
|---|---|
| 正极（长脚 / +） | **GPIO5** |
| 负极（短脚 / −） | GND |

**停止闹钟**：板载 **BOOT** 键（GPIO0），闹钟响的时候按一下。

> ⚠️ 蜂鸣器如果是 5V 型号，**必须**用 S8050 三极管 + 独立 5V 供电，把 GPIO 接基极串 1kΩ。
> 直接拿 GPIO 驱动 5V 蜂鸣器会拉低电压导致板子复位。

引脚定义集中在 `main/app_cfg.h`，想改随时改：

```c
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9
#define PIN_BUZZER  5
#define PIN_KEY     0
```

---

## 3. 目录结构

```
weather_clock_s2/
├── CMakeLists.txt
├── sdkconfig.defaults          8MB Flash / 证书包 / 栈深等编译配置
├── partitions.csv              3MB app 分区
├── main/
│   ├── app_cfg.h               引脚与周期常量
│   ├── app_data.h              各任务共享的显示数据
│   ├── main.c                  启动流程 + 渲染任务 + 周期任务
│   ├── cfg.c/h                 NVS 配置读写
│   ├── ssd1306.c/h             OLED 驱动（v6.0 新版 i2c_master）
│   ├── gfx.c/h                 帧缓冲绘图引擎
│   ├── form.c/h                表单解析（配网页用）
│   ├── wifi.c/h                WiFi 连接 / 重连 / 扫描
│   ├── prov.c/h                SoftAP 网页配网
│   ├── net_http.c/h            HTTPS GET 封装
│   ├── weather.c/h             心知天气
│   ├── bili.c/h                B站数据
│   ├── mqtt_app.c/h            MQTT 控制
│   ├── buzzer.c/h              蜂鸣器 + 闹钟
│   ├── ui.c/h                  4 个页面的渲染
│   └── fonts/
│       ├── font.h font.c       字库查找 + UTF-8 解码
│       ├── font_ascii_8x16.c   ASCII/符号 8×16
│       ├── font_num_24x32.c    大号数字 24×32
│       └── font_cn_16x16.c     GB2312 汉字 16×16（4437 字）
├── tools/
│   ├── gen_font.py             字库生成器（含界面预览 + 越界校验）
│   ├── host_test.c             主机侧渲染测试
│   ├── test_units.c            单元测试
│   └── stub/                   IDF 桩头文件（仅用于主机编译检查）
└── docs/                       界面预览图
```

---

## 4. 编译与烧录

```bash
# 1) 载入 IDF 环境（按你的实际路径）
. $HOME/esp/esp-idf/export.sh

cd weather_clock_s2

# 2) 选定芯片（只需一次）
idf.py set-target esp32s2

# 3) 编译。首次会联网拉取 esp-mqtt 组件
idf.py build

# 4) 烧录 + 看日志
idf.py -p /dev/ttyUSB0 flash monitor
```

- 本项目按**板载 UART**（CP210x）走，端口是 `/dev/ttyUSB0`；若走 S2 原生 USB 直连则是 `/dev/ttyACM0`。
  用 `ls /dev/ttyUSB* /dev/ttyACM*` 确认，`-p` 参数填实际出现的那个。
- 虚拟机里需要把 USB 设备直通给 Linux（VMware：虚拟机 → 可移动设备 → 连接；VirtualBox：设备 → USB）。
- 烧录不进时：按住 **BOOT** 键，点一下 **RST**，松开 BOOT，再执行烧录。

> **v6.0 的两个坑**（本项目已处理，改代码时注意）：
> 1. `esp-mqtt` 已移出 IDF，必须靠 `main/idf_component.yml` 里的 `espressif/mqtt` 拉取；
> 2. 驱动头文件不再隐式包含 FreeRTOS 头，用到 `vTaskDelay` 必须自己 `#include "freertos/task.h"`。

---

## 5. 首次配网

1. 烧录完成后设备屏幕显示 `配网模式 / WC-XXXX / 192.168.4.1`。
2. 手机连热点 **WC-XXXX**（无密码）。
3. 浏览器打开 **http://192.168.4.1**。
4. 填写：
   - **WiFi 名称/密码**：从扫描列表选，或手动输入（区分大小写）
   - **心知天气私钥**：到 [seniverse.com](https://www.seniverse.com/) 免费注册，进控制台拿私钥
   - **城市代码**：`beijing` / `shanghai` / `hangzhou` …（拼音即可）
   - **B站 UID / BV 号**：可留空
   - **MQTT 服务器**：默认 `mqtt://test.ranye-iot.net:1883`（原教程的公开测试服务器）
   - **亮度 / 轮播间隔 / 闹钟时间**
5. 点"保存并重启"，设备自动重启并联网。

> 以后想换 WiFi：**长按 BOOT 键 5 秒**，清除 WiFi 配置并重启进入配网模式。
> 另外，若开机 20 秒内连不上原 WiFi，设备也会自动回到配网模式。

---

## 6. MQTT 使用

设备连上 broker 后，串口会打印三个主题（MAC 后 3 字节，你的和别人的不会重复）：

```
控制主题: wc/A1B2C3/control
聊天主题: wc/A1B2C3/chat
上报主题: wc/A1B2C3/report
```

用任意 MQTT 客户端（推荐 [MQTTX](https://mqttx.app/)）连到同一个 broker，向 **control** 主题发指令：

| 指令 | 作用 |
|---|---|
| `hb` | 立刻上报一次状态到 report 主题 |
| `msg 明早记得带伞` | 屏幕全屏弹出该消息，10 秒后恢复 |
| `page 0` | 切到时钟页（0 时钟 / 1 天气 / 2 B站 / 3 消息） |
| `buz 1` | 蜂鸣器长响（门铃/报警用法），`buz 0` 关闭 |
| `alarm 07:30 on` | 设置并开启闹钟 |
| `stop` | 停止正在响的闹钟 |

向 **chat** 主题发任意文本，效果等同于 `msg`。

report 主题会收到 JSON：

```json
{"wifi":"192.168.1.23","heap":182344,"weather":"晴 23C","alarm":"07:00","en":1,"ring":0}
```

---

## 7. 界面说明

| 页面 | 内容 |
|---|---|
| 0 时钟 | `11月03日周二` + 右上角秒数 + 巨型 `09:41` + 底部天气/气温/风向 |
| 1 天气 | 城市+天气、实时温度+今日温度区间、风向风力、更新时间 |
| 2 B站 | UP 主粉丝数、视频播放量、BV 号 |
| 3 消息 | MQTT 最近 3 条消息 |

轮播顺序 `时钟 → 天气 → 时钟 → B站`，间隔在配网页可调（默认 6 秒）。
没有配置 B站时自动跳过 B站页；收到新消息会临时占用屏幕 10 秒。

界面预览图见 `docs/preview_pages.png`、`docs/host_pages.png`（后者是固件绘图代码真实渲染出来的）。

---

## 8. 想改字库/界面？

中文点阵是脚本生成的，不是手画的：

```bash
# Windows 或 Linux 都可以，需要 Python3 + Pillow
pip install pillow
python tools/gen_font.py --preview
```

- `--preview` 会额外输出 `docs/preview_pages.png`，并**自动校验**文字是否缺字、是否超出 128px 边界。
- 想加汉字 → 改 `tools/gen_font.py` 里 `gb2312_chars()` 的区段（当前是 GB2312 符号区 + 一级常用字，共 4437 字）。
- Linux 下如果报"找不到字体"，装一个中文字体并把 `WINFONT` 指向字体目录：
  `export WINFONT=/usr/share/fonts/truetype/wqy`

改界面的坐标时，`tools/gen_font.py` 的 `draw_preview()` 和 `main/ui.c` 要同步改（两边是同一套坐标）。

---

## 9. 主机侧自测（不用板子）

工程带了一套"脱离硬件"的验证工具，改完代码可以先在电脑上跑一遍：

```bash
# 语法检查（用桩头文件模拟 IDF API）
for f in main/*.c main/fonts/font.c; do
  gcc -std=gnu11 -fsyntax-only -Wall -Wextra -I tools/stub -I main -I main/fonts "$f"
done

# 单元测试：表单解析 / UTF-8 / 字库二分查找 / 绘图边界
gcc -std=gnu11 -O1 -I main -I main/fonts main/form.c main/gfx.c \
    main/fonts/*.c tools/test_units.c -o build/test_units && ./build/test_units

# 渲染 4 个页面成图片，肉眼检查排版
gcc -std=gnu11 -O1 -I main -I main/fonts main/gfx.c main/fonts/*.c \
    main/ui.c tools/host_test.c -o build/host_test && ./build/host_test
```

---

## 10. 常见问题

**屏幕全黑**
1. 串口有没有打印 `SSD1306 已找到，地址 0x3C`？没有就是接线/供电问题。
2. 确认 VCC 接的是 3V3 不是 5V。
3. 有些模块 SDA/SCL 丝印标反了，对调试一下。
4. 极少数模块地址是 0x3D——代码会自动探测，看日志确认。

**显示中文乱码/问号**
字库里没有那个字。当前收录 GB2312 符号区 + 一级汉字，绝大多数日常用字都有；生僻字需要用 `gen_font.py` 重新生成。

**天气一直是"暂无数据"**
- 心知私钥/城市填错 → 串口会打印 `心知返回异常，检查私钥/城市是否正确`
- 免费账号有调用次数限制，本项目 15 分钟一次，一天 96 次，够用
- 检查是否做过 NTP 对时：HTTPS 证书校验需要正确时间，时间不对会直接握手失败

**B站数据取不到**
接口返回 `code=-412` 或 `-352` 是风控。项目已带浏览器 UA；若仍失败，多半是 IP 被限流，过一会儿再试。

**MQTT 连不上 `test.ranye-iot.net`**
那是公开测试服务器，可能被挤爆或下线。配网页里把 MQTT 改成你自己的 broker（例如自建 mosquitto，或 `mqtt://broker.emqx.io:1883`）。

**闹钟不响 / 一直响**
- 不响：检查配网页闹钟是否"开启"，且设备是否已完成 NTP 对时（没对时就没有正确时间）。
- 一直响：按 BOOT 键停止；响满 60 秒也会自动停。

**编译报 `mqtt_client.h: No such file`**
组件没拉下来。确认 `main/idf_component.yml` 存在，且虚拟机联网，然后 `idf.py reconfigure`。
