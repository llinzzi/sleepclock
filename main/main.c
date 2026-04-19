#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "wifi_connect.h"
#include "ssd1322_driver.h"
#include "lvgl_adapter.h"
#include "ui.h"
#include "lvgl.h"
#include "screen_mgr.h"
#include "btn_handler.h"
#include "weather_service.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Sleep Clock");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SSD1322 driver
    ESP_ERROR_CHECK(ssd1322_init());

    // Initialize LVGL adapter
    ESP_ERROR_CHECK(lvgl_adapter_init());

    // Initialize UI (bypassing EEZ Studio UI)
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(ui_wrapper_init());
    vTaskDelay(pdMS_TO_TICKS(100));

    // Initialize button handler
    btn_handler_init();

    // Initialize screen manager
    screen_mgr_init();

    // Initialize weather service
    weather_service_init();

    // Connect to WiFi for weather data
    wifi_connect();

    ESP_LOGI(TAG, "All initialized successfully");

    // Main loop - handle buttons and update screen
    uint32_t last_update = 0;
    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Update screen every ~16ms (60fps)
        if (now - last_update >= 16) {
            btn_event_t event = btn_handler_poll();

            if (event == BTN_EVENT_PRESS) {
                ESP_LOGI(TAG, "BTN_EVENT_PRESS - toggling mode");
                screen_mgr_on_wake();  // Wake up and show appropriate mode
            } else if (event == BTN_EVENT_LONG_3S) {
                ESP_LOGI(TAG, "Button long 3s - sleep");
                screen_mgr_on_sleep();
            }

            // Update screen manager (renders to framebuffer)
            screen_mgr_update(now);

            // Trigger direct display refresh
            ssd1322_flush_framebuffer();

            last_update = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
