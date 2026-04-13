# Sleep Clock

基于 ESP-IDF 5.5 框架的睡眠时钟固件，驱动 256x64 SSD1322 灰度 OLED 显示屏。

## 硬件规格

### 主控
- **芯片**: ESP32-C3-MINI-1 (WiFi/Bluetooth)
- **USB-UART**: CP2102N
- **工作电压**: 3.3V

### 显示
- **型号**: SSD1322
- **分辨率**: 256x64 灰度 OLED

### GPIO 连接

| GPIO | 功能 | 连接到 |
|------|------|--------|
| GPIO0 | LED_CTRL | LED 驱动（通过 Q1） |
| GPIO2 | NS_CTRL | 音频功放控制 |
| GPIO3 | KEY | 用户按键 |
| GPIO4 | I2S_SDIN | 音频功放 NS4168 |
| GPIO5 | I2S_SCLK | 音频功放 NS4168 |
| GPIO6 | I2S_LROUT | 音频功放 NS4168 |
| GPIO7 | SPI_SCK | SSD1322 SCLK |
| GPIO8 | SPI_DC | SSD1322 DC |
| GPIO10 | SPI_SDA | SSD1322 SDI |
| GPIO18 | USB_D- | USB 数据线 |
| GPIO19 | USB_D+ | USB 数据线 |
| GPIO20 | IN1 | LED 驱动 FM116C |
| GPIO21 | IN2 | LED 驱动 FM116C |

> SPI_CS 硬件接地，无需 GPIO。SPI_RST 连接到 CHIP_PU 复位信号。

## 功能特性

- LVGL v9.4.0 UI 渲染
- EEZ Studio 生成的 UI 界面
- I4 格式 (4-bit 灰度) 显示输出
- SPI 通信 (10MHz)

## 构建

```bash
# 设置目标芯片
idf.py set-target esp32c3

# 配置项目
idf.py menuconfig

# 编译
idf.py build

# 烧录
idf.py -p /dev/ttyUSB0 flash

# 串口监视
idf.py -p /dev/ttyUSB0 monitor
```

## 项目架构

```
main/
├── ssd1322_driver.c/h   # SSD1322 低层驱动
├── lvgl_adapter.c/h     # LVGL 显示适配器
├── ui_wrapper.c/h       # UI 初始化封装
└── main.c               # 入口函数

ui/                      # EEZ Studio 生成的 UI 文件
├── screens.c/h
├── styles.c
├── images.c
└── ui.c
```

## 显示接口说明

SSD1322 使用 I4 格式（4-bit 灰度，2 像素/字节）。LVGL 渲染使用 L8 格式（8-bit 灰度）。`lvgl_adapter.c` 中的 `lvgl_flush_cb()` 完成 L8 到 I4 的格式转换。

---

*原理图版本 1.9 | 2026-04-13*
