#include "ui.h"
#include "lvgl_adapter.h"
#include "esp_log.h"

static const char *TAG = "UI_WRAPPER";

esp_err_t ui_wrapper_init(void)
{
    // 调用eez studio生成的UI初始化
    ui_init();
    
    // 通知LVGL适配器UI已就绪
    lvgl_adapter_set_ui_ready();
    
    ESP_LOGI(TAG, "EEZ UI initialized successfully");
    return ESP_OK;
}
