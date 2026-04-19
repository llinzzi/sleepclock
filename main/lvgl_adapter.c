#include "lvgl_adapter.h"
#include "ssd1322_driver.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LVGL_ADAPTER";
static lv_display_t *g_disp = NULL;
static uint8_t *g_i4_buffer = NULL;  // 静态I4缓冲区
static uint8_t *g_framebuffer = NULL;  // L8帧缓冲区
static bool screen_dirty = false;  // 屏幕需要刷新标志
static uint8_t *g_full_i4_buf = NULL;  // 全屏I4缓冲区

// 函数声明
static void lvgl_task(void *arg);

// LVGL flush回调 - L8格式转I4
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int x_start = area->x1;
    int y_start = area->y1;
    int x_end = area->x2;
    int y_end = area->y2;
    
    int width = x_end - x_start + 1;
    int height = y_end - y_start + 1;
    size_t i4_len = (width / 2) * height;
    
    // 使用静态缓冲区，避免频繁分配
    if (g_i4_buffer) {
        // 转换L8到I4：每2个L8像素合并为1个I4字节
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += 2) {
                int src_idx = y * width + x;
                int dst_idx = y * (width / 2) + (x / 2);
                
                uint8_t p0 = px_map[src_idx] >> 4;      // 第1个像素，取高4位
                uint8_t p1 = px_map[src_idx + 1] >> 4;  // 第2个像素，取高4位
                
                g_i4_buffer[dst_idx] = (p0 << 4) | p1;  // 合并为1字节
            }
        }
        
        // SSD1322列地址 = 像素列数/4，每地址4像素
        ssd1322_send_cmd(0x15);
        ssd1322_send_data((x_start / 4) & 0x3F);  // 列起始 (0-63)
        ssd1322_send_data(((x_end / 4)) & 0x3F);  // 列结束
        
        ssd1322_send_cmd(0x75);
        ssd1322_send_data(y_start);
        ssd1322_send_data(y_end);
        
        ssd1322_send_cmd(0x5C); // Write RAM
        
        gpio_set_level(PIN_NUM_DC, 1);
        
        spi_transaction_t t = {
            .length = i4_len * 8,
            .tx_buffer = g_i4_buffer
        };
        spi_device_polling_transmit(ssd1322_get_spi_handle(), &t);
    }
    
    lv_display_flush_ready(disp);
}

esp_err_t lvgl_adapter_init(void)
{
    // 初始化LVGL
    lv_init();
    
    // 手动创建LVGL显示器
    g_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (!g_disp) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return ESP_FAIL;
    }
    
    // 设置颜色格式为L8（8位灰度）
    lv_display_set_color_format(g_disp, LV_COLOR_FORMAT_L8);
    
    // 分配I4转换缓冲区
    g_i4_buffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES / 2, MALLOC_CAP_DMA);
    if (!g_i4_buffer) {
        ESP_LOGE(TAG, "Failed to allocate I4 buffer");
        return ESP_ERR_NO_MEM;
    }

    // 分配全屏I4缓冲区用于直接刷新
    g_full_i4_buf = heap_caps_malloc(LCD_H_RES * LCD_V_RES / 2, MALLOC_CAP_DMA);
    if (!g_full_i4_buf) {
        ESP_LOGE(TAG, "Failed to allocate full I4 buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // 分配LVGL缓冲区（L8格式）
    size_t buf_size = LCD_H_RES * LCD_V_RES;
    g_framebuffer = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!g_framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate LVGL buffer");
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_buffers(g_disp, g_framebuffer, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // 设置flush回调
    lv_display_set_flush_cb(g_disp, lvgl_flush_cb);
    
    // 创建LVGL任务
    xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "LVGL adapter initialized");
    return ESP_OK;
}

// LVGL定时任务
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    while (1) {
        lv_timer_handler();
        // 不调用 ui_tick()，因为 EEZ UI 未初始化
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

lv_display_t* lvgl_adapter_get_display(void)
{
    return g_disp;
}

uint8_t* lvgl_get_framebuffer(void)
{
    return g_framebuffer;
}

// LVGL绘制像素
void lvgl_draw_pixel(int x, int y, uint8_t brightness) {
    if (x < 0 || x >= LCD_H_RES || y < 0 || y >= LCD_V_RES) return;
    if (brightness > 15) brightness = 15;
    g_framebuffer[y * LCD_H_RES + x] = brightness << 4;  // L8 format: high nibble for I4
    screen_dirty = true;
}

void lvgl_draw_line(int x1, int y1, int x2, int y2, uint8_t brightness) {
    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        lvgl_draw_pixel(x1, y1, brightness);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void lvgl_draw_circle(int x, int y, int radius, uint8_t brightness) {
    // Midpoint circle algorithm
    int x0 = 0;
    int y0 = radius;
    int d = 1 - radius;

    while (x0 <= y0) {
        lvgl_draw_pixel(x + x0, y + y0, brightness);
        lvgl_draw_pixel(x - x0, y + y0, brightness);
        lvgl_draw_pixel(x + x0, y - y0, brightness);
        lvgl_draw_pixel(x - x0, y - y0, brightness);
        lvgl_draw_pixel(x + y0, y + x0, brightness);
        lvgl_draw_pixel(x - y0, y + x0, brightness);
        lvgl_draw_pixel(x + y0, y - x0, brightness);
        lvgl_draw_pixel(x - y0, y - x0, brightness);
        if (d < 0) {
            d += 2 * x0 + 3;
        } else {
            d += 2 * (x0 - y0) + 5;
            y0--;
        }
        x0++;
    }
}

void lvgl_clear(uint8_t brightness) {
    if (brightness > 15) brightness = 15;
    uint8_t val = brightness << 4;
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        g_framebuffer[i] = val;
    }
    screen_dirty = true;
}

void lvgl_trigger_refresh(void) {
    if (!screen_dirty) return;
    screen_dirty = false;

    // 直接渲染全屏到SSD1322（绕过LVGL flush机制）
    if (g_framebuffer && g_full_i4_buf) {
        // L8转I4：每2个L8像素合并为1个I4字节
        for (int y = 0; y < LCD_V_RES; y++) {
            for (int x = 0; x < LCD_H_RES; x += 2) {
                int src_idx = y * LCD_H_RES + x;
                int dst_idx = y * (LCD_H_RES / 2) + (x / 2);
                uint8_t p0 = g_framebuffer[src_idx] >> 4;      // 第1个像素，取高4位
                uint8_t p1 = g_framebuffer[src_idx + 1] >> 4;  // 第2个像素，取高4位
                g_full_i4_buf[dst_idx] = (p0 << 4) | p1;
            }
        }

        // 设置显示区域为全屏 (256x64像素 = 64列 x 4像素/列)
        ssd1322_send_cmd(0x15);
        ssd1322_send_data(0x00);  // 列起始
        ssd1322_send_data(0x3F);  // 列结束 (0x3F = 63, 64列)

        ssd1322_send_cmd(0x75);
        ssd1322_send_data(0x00);  // 行起始
        ssd1322_send_data(0x3F);  // 行结束 (63)

        ssd1322_send_cmd(0x5C); // Write RAM

        gpio_set_level(PIN_NUM_DC, 1);

        spi_transaction_t t = {
            .length = LCD_H_RES * LCD_V_RES / 2 * 8,
            .tx_buffer = g_full_i4_buf
        };
        spi_device_polling_transmit(ssd1322_get_spi_handle(), &t);
    }
}

// Simple 5x7 pixel font for digits and colon
// Each character is 5 pixels wide, 7 pixels tall, stored as bitmask
static const uint8_t font_5x7[12][7] = {
    // 0
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    // 1
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    // 2
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},
    // 3
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    // 4
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    // 5
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    // 6
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    // 7
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    // 8
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    // 9
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    // : (index 10)
    {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
    // ° (degree, index 11)
    {0x04, 0x0A, 0x0A, 0x04, 0x00, 0x00, 0x00},
};

void lvgl_draw_text(int x, int y, const char *str, uint8_t font_size, uint8_t brightness, bool bold) {
    if (str == NULL || brightness < 2) return;

    // Scale factor for larger fonts
    int scale = font_size / 7;
    if (scale < 1) scale = 1;

    int cur_x = x;
    int cur_y = y;

    while (*str) {
        char c = *str;
        int char_idx = -1;

        if (c >= '0' && c <= '9') {
            char_idx = c - '0';
        } else if (c == ':') {
            char_idx = 10;
        } else if (c == 0xB0) {  // Degree symbol
            char_idx = 11;
        }

        if (char_idx >= 0 && char_idx < 12) {
            // Draw each column of the character
            for (int col = 0; col < 5; col++) {
                uint8_t col_data = font_5x7[char_idx][col];
                for (int row = 0; row < 7; row++) {
                    if (col_data & (1 << row)) {
                        // Draw pixel at scaled position
                        for (int sx = 0; sx < scale; sx++) {
                            for (int sy = 0; sy < scale; sy++) {
                                lvgl_draw_pixel(cur_x + col * scale + sx,
                                               cur_y + row * scale + sy,
                                               brightness);
                            }
                        }
                        if (bold) {
                            // Extra pixel for bold
                            for (int sx = 0; sx < scale; sx++) {
                                for (int sy = 0; sy < scale; sy++) {
                                    lvgl_draw_pixel(cur_x + col * scale + sx + 1,
                                                   cur_y + row * scale + sy,
                                                   brightness);
                                }
                            }
                        }
                    }
                }
            }
            cur_x += (5 + 1) * scale;  // char width + 1 pixel spacing
        }
        str++;
    }
}
