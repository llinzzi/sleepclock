#ifndef UI_WRAPPER_H
#define UI_WRAPPER_H

#include "esp_err.h"
#include "ui/ui.h"  // eez studio生成的UI

/**
 * @brief 初始化UI界面（包装eez UI）
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t ui_wrapper_init(void);

#endif // UI_WRAPPER_H
