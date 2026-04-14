#ifndef LVGL_ADAPTER_H
#define LVGL_ADAPTER_H

#include "lvgl.h"
#include "esp_err.h"

/**
 * @brief 初始化LVGL适配层
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t lvgl_adapter_init(void);

/**
 * @brief 获取LVGL显示器对象
 * @return LVGL显示器对象指针
 */
lv_display_t* lvgl_adapter_get_display(void);

/**
 * @brief 设置UI已准备好
 */
void lvgl_adapter_set_ui_ready(void);

/**
 * @brief 绘制像素点
 */
void lvgl_draw_pixel(int x, int y, uint8_t brightness);

/**
 * @brief 绘制直线
 */
void lvgl_draw_line(int x1, int y1, int x2, int y2, uint8_t brightness);

/**
 * @brief 绘制圆点
 */
void lvgl_draw_circle(int x, int y, int radius, uint8_t brightness);

/**
 * @brief 绘制文字
 */
void lvgl_draw_text(int x, int y, const char *str, uint8_t font_size, uint8_t brightness, bool bold);

/**
 * @brief 清除屏幕
 */
void lvgl_clear(uint8_t brightness);

/**
 * @brief 获取帧缓冲区指针
 */
uint8_t* lvgl_get_framebuffer(void);

/**
 * @brief 触发屏幕刷新
 */
void lvgl_trigger_refresh(void);

#endif // LVGL_ADAPTER_H
