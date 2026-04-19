#include "btn_handler.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <sys/time.h>

static const char *TAG = "BTN_HANDLER";

#define BTN_GPIO GPIO_NUM_3
#define DEBOUNCE_MS 50
#define LONG_3S_MS 3000
#define LONG_10S_MS 10000

typedef enum {
    BTN_STATE_IDLE,
    BTN_STATE_PRESSED,
    BTN_STATE_LONG_3S,
    BTN_STATE_LONG_10S,
} btn_state_t;

static btn_state_t btn_state = BTN_STATE_IDLE;
static int64_t press_start_time = 0;
static bool btn_initialized = false;

void btn_handler_init(void) {
    if (btn_initialized) return;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    btn_state = BTN_STATE_IDLE;
    btn_initialized = true;
    ESP_LOGI(TAG, "Button handler init on GPIO%d", BTN_GPIO);
}

btn_event_t btn_handler_poll(void) {
    if (!btn_initialized) {
        btn_handler_init();
    }

    int btn_level = gpio_get_level(BTN_GPIO);
    int64_t now = esp_timer_get_time() / 1000;  // Convert to ms

    switch (btn_state) {
        case BTN_STATE_IDLE:
            if (btn_level == 0) {  // Button pressed (active low)
                press_start_time = now;
                btn_state = BTN_STATE_PRESSED;
                ESP_LOGI(TAG, "Button pressed");
            }
            break;

        case BTN_STATE_PRESSED:
            if (btn_level == 1) {  // Button released
                btn_state = BTN_STATE_IDLE;
                ESP_LOGI(TAG, "Button short press - returning BTN_EVENT_PRESS");
                return BTN_EVENT_PRESS;
            }
            if ((now - press_start_time) >= LONG_10S_MS) {
                btn_state = BTN_STATE_LONG_10S;
            } else if ((now - press_start_time) >= LONG_3S_MS) {
                btn_state = BTN_STATE_LONG_3S;
            }
            break;

        case BTN_STATE_LONG_3S:
            if (btn_level == 1) {  // Button released after long press
                btn_state = BTN_STATE_IDLE;
                return BTN_EVENT_LONG_3S;
            }
            if ((now - press_start_time) >= LONG_10S_MS) {
                btn_state = BTN_STATE_LONG_10S;
            }
            break;

        case BTN_STATE_LONG_10S:
            if (btn_level == 1) {  // Button released after long press
                btn_state = BTN_STATE_IDLE;
                return BTN_EVENT_LONG_10S;
            }
            break;
    }

    return BTN_EVENT_NONE;
}
