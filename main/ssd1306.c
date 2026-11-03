#include <string.h>

#include "freertos/FreeRTOS.h"     /* v6.0 起驱动头文件不再隐式包含 FreeRTOS 头 */
#include "freertos/task.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

#include "app_cfg.h"
#include "ssd1306.h"

static const char *TAG = "oled";

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static uint8_t s_tx[OLED_FB_SIZE + 1];   /* 首字节为 0x40 数据控制符 */

/* SSD1306 128x64 上电初始化序列，逐字节发送（控制符 0x00 = 整包命令） */
static const uint8_t s_init_seq[] = {
    0xAE,        /* 关显示 */
    0xD5, 0x80,  /* 时钟分频 */
    0xA8, 0x3F,  /* 复用率 = 64 行 */
    0xD3, 0x00,  /* 显示偏移 0 */
    0x40,        /* 起始行 0 */
    0x8D, 0x14,  /* 电荷泵开 */
    0x20, 0x00,  /* 水平寻址模式 */
    0xA1,        /* 段重映射（左右翻转，适配常见模块走线） */
    0xC8,        /* COM 反向扫描 */
    0xDA, 0x12,  /* COM 引脚配置 */
    0x81, 0xCF,  /* 对比度 */
    0xD9, 0xF1,  /* 预充电周期 */
    0xDB, 0x40,  /* VCOMH */
    0xA4,        /* 显示跟随 RAM */
    0xA6,        /* 正常显示（非反色） */
    0x2E,        /* 关滚动 */
    0xAF,        /* 开显示 */
};

static esp_err_t send_cmds(const uint8_t *cmds, size_t n)
{
    uint8_t buf[32];
    if (n + 1 > sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    buf[0] = 0x00;
    memcpy(&buf[1], cmds, n);
    return i2c_master_transmit(s_dev, buf, n + 1, 100);
}

esp_err_t ssd1306_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "创建 I2C 总线失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 0x3C 是绝大多数模块的地址，个别模块是 0x3D，这里自动探测 */
    uint8_t addr = OLED_ADDR;
    if (i2c_master_probe(s_bus, addr, 100) != ESP_OK) {
        if (i2c_master_probe(s_bus, 0x3D, 100) == ESP_OK) {
            addr = 0x3D;
        } else {
            ESP_LOGE(TAG, "I2C 上找不到 SSD1306（SDA=%d SCL=%d），请检查接线",
                     PIN_I2C_SDA, PIN_I2C_SCL);
            return ESP_ERR_NOT_FOUND;
        }
    }
    ESP_LOGI(TAG, "SSD1306 已找到，地址 0x%02X", addr);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "挂载 I2C 设备失败: %s", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(50));   /* 等 OLED 电源稳定 */
    err = send_cmds(s_init_seq, sizeof(s_init_seq));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "初始化命令发送失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 清屏 */
    static uint8_t zero[OLED_FB_SIZE];
    return ssd1306_flush(zero);
}

esp_err_t ssd1306_flush(const uint8_t *fb)
{
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    /* 水平寻址模式下的整屏窗口 */
    const uint8_t win[] = {
        0x21, 0x00, OLED_W - 1,
        0x22, 0x00, (OLED_H / 8) - 1,
    };
    esp_err_t err = send_cmds(win, sizeof(win));
    if (err != ESP_OK) {
        return err;
    }

    s_tx[0] = 0x40;   /* 后续为显存数据 */
    memcpy(&s_tx[1], fb, OLED_FB_SIZE);
    return i2c_master_transmit(s_dev, s_tx, sizeof(s_tx), 500);
}

esp_err_t ssd1306_set_contrast(uint8_t value)
{
    const uint8_t c[] = {0x81, value};
    return send_cmds(c, sizeof(c));
}

esp_err_t ssd1306_display_on(bool on)
{
    const uint8_t c[] = {on ? 0xAF : 0xAE};
    return send_cmds(c, sizeof(c));
}
