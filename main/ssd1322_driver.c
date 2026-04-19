#include "ssd1322_driver.h"
#include "lvgl_adapter.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SSD1322_DRV";
static spi_device_handle_t g_spi = NULL;

void ssd1322_send_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_NUM_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd
    };
    spi_device_polling_transmit(g_spi, &t);
}

void ssd1322_send_data(uint8_t data)
{
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data
    };
    spi_device_polling_transmit(g_spi, &t);
}

spi_device_handle_t ssd1322_get_spi_handle(void)
{
    return g_spi;
}

esp_err_t ssd1322_flush_framebuffer(void)
{
    uint8_t *fb = lvgl_get_framebuffer();
    if (!fb) return ESP_FAIL;

    // L8转I4：每2个L8像素合并为1个I4字节
    static uint8_t i4_buf[LCD_H_RES * LCD_V_RES / 2];
    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x += 2) {
            int src_idx = y * LCD_H_RES + x;
            int dst_idx = y * (LCD_H_RES / 2) + (x / 2);
            uint8_t p0 = fb[src_idx] >> 4;       // 第1个像素，取高4位
            uint8_t p1 = fb[src_idx + 1] >> 4;  // 第2个像素，取高4位
            i4_buf[dst_idx] = (p0 << 4) | p1;
        }
    }

    // 设置显示区域为全屏 (256x64像素 = 64列地址 x 4像素 + 64行)
    // 列地址 0x00-0x3F = 64组 x 4像素/组 = 256像素
    ssd1322_send_cmd(0x15);
    ssd1322_send_data(0x00);  // 列起始
    ssd1322_send_data(0x3F);  // 列结束 (0x3F = 63, 64组)

    ssd1322_send_cmd(0x75);
    ssd1322_send_data(0x00);  // 行起始
    ssd1322_send_data(0x3F);  // 行结束 (63, 64行)

    ssd1322_send_cmd(0x5C);   // Write RAM

    gpio_set_level(PIN_NUM_DC, 1);

    spi_transaction_t t = {
        .length = LCD_H_RES * LCD_V_RES / 2 * 8,
        .tx_buffer = i4_buf
    };
    spi_device_polling_transmit(g_spi, &t);

    return ESP_OK;
}

esp_err_t ssd1322_init(void)
{
    esp_err_t ret;
    
    // 配置GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed");
        return ret;
    }
    
    // 配置SPI总线
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES,  // 256*64 = 16384字节
    };
    ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return ret;
    }
    
    // 添加SPI设备
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
    };
    ret = spi_bus_add_device(LCD_HOST, &devcfg, &g_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed");
        return ret;
    }
    
    // 硬件复位
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 初始化SSD1322寄存器
    ssd1322_send_cmd(0xFD); ssd1322_send_data(0x12);
    ssd1322_send_cmd(0xAE);
    ssd1322_send_cmd(0xB3); ssd1322_send_data(0x91);
    ssd1322_send_cmd(0xCA); ssd1322_send_data(0x3F);
    ssd1322_send_cmd(0xA2); ssd1322_send_data(0x00);
    ssd1322_send_cmd(0xA1); ssd1322_send_data(0x00);
    ssd1322_send_cmd(0xA0); ssd1322_send_data(0x14); ssd1322_send_data(0x11);
    ssd1322_send_cmd(0xAB); ssd1322_send_data(0x01);
    ssd1322_send_cmd(0xB4); ssd1322_send_data(0xA0); ssd1322_send_data(0xFD);
    ssd1322_send_cmd(0xC1); ssd1322_send_data(0x80);
    ssd1322_send_cmd(0xC7); ssd1322_send_data(0x0F);
    ssd1322_send_cmd(0xB1); ssd1322_send_data(0xE2);
    ssd1322_send_cmd(0xD1); ssd1322_send_data(0x82); ssd1322_send_data(0x20);
    ssd1322_send_cmd(0xBB); ssd1322_send_data(0x1F);
    ssd1322_send_cmd(0xB6); ssd1322_send_data(0x08);
    ssd1322_send_cmd(0xBE); ssd1322_send_data(0x07);
    ssd1322_send_cmd(0xA6);
    ssd1322_send_cmd(0xAF);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "SSD1322 initialized");
    
    return ESP_OK;
}
