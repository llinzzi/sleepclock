#include "ui.h"
#include "lvgl_adapter.h"
#include "esp_log.h"
#include "ssd1322_driver.h"

static const char *TAG = "UI_WRAPPER";

esp_err_t ui_wrapper_init(void)
{
    // 不调用 ui_init()，因为 EEZ Studio 生成的 UI 代码与 LVGL v9 不兼容
    // 直接创建空白屏幕

    // 创建一个空白屏幕用于 LVGL 内部使用
    lv_obj_t *blank_screen = lv_obj_create(NULL);
    lv_obj_set_size(blank_screen, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_bg_color(blank_screen, lv_color_make(0, 0, 0), 0);
    lv_scr_load(blank_screen);

    // 通知LVGL适配器UI已就绪
    // 注: EEZ UI 已禁用，无需调用 set_ui_ready

    ESP_LOGI(TAG, "UI wrapper initialized (bypassing EEZ UI)");
    return ESP_OK;
}
