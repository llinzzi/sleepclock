#include "screen_mgr.h"
#include "lvgl_adapter.h"
#include "star_field.h"
#include "weather_chart.h"
#include "weather_service.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <sys/time.h>

static const char *TAG = "SCREEN_MGR";

static screen_mode_t current_mode = SCREEN_SLEEP;
static uint8_t brightness = 0;
static float breath_phase = 0;
static uint32_t mode_start_time = 0;
static uint32_t rain_anim_phase = 0;

void screen_mgr_init(void) {
    ESP_LOGI(TAG, "Screen manager init");
    star_field_init();
    weather_chart_init();
    current_mode = SCREEN_SLEEP;
    brightness = 0;
    breath_phase = 0;
    mode_start_time = 0;
}

void screen_mgr_set_mode(screen_mode_t mode) {
    if (current_mode == mode) return;
    current_mode = mode;
    mode_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "Mode changed to %d", mode);
}

void screen_mgr_on_wake(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int hour = (tv.tv_sec % 86400) / 3600;

    // 18:00-06:59 is star mode, 07:00-17:59 is weather mode
    if (hour >= 18 || hour < 7) {
        screen_mgr_set_mode(SCREEN_STAR);
    } else {
        screen_mgr_set_mode(SCREEN_WEATHER);
    }
}

void screen_mgr_on_sleep(void) {
    screen_mgr_set_mode(SCREEN_SLEEP);
    brightness = 0;
}

uint32_t get_elapsed_ms(void) {
    return (xTaskGetTickCount() * portTICK_PERIOD_MS) - mode_start_time;
}

void screen_mgr_update(uint32_t elapsed_ms) {
    // Update breath phase for animations
    breath_phase += 0.02f;
    if (breath_phase > 2 * M_PI) {
        breath_phase -= 2 * M_PI;
    }

    // Update rain animation phase
    rain_anim_phase += 10;
    if (rain_anim_phase > 1000) {
        rain_anim_phase = 0;
    }

    // Clear screen
    lvgl_clear(0);

    switch (current_mode) {
        case SCREEN_SLEEP:
            // Dim ambient noise - simplified, just fade out
            brightness = (brightness > 0) ? brightness - 1 : 0;
            if (brightness > 0) {
                // Draw some random noise pixels
                for (int i = 0; i < 20; i++) {
                    int x = rand() % 256;
                    int y = rand() % 64;
                    lvgl_draw_pixel(x, y, 1);
                }
            }
            break;

        case SCREEN_WAKE:
            // Auto-select mode based on time (handled in on_wake)
            break;

        case SCREEN_STAR:
            {
                uint32_t elapsed = get_elapsed_ms();

                // Stars: fade in 0-3s, hold 3-13s, fade out 13-15s
                uint8_t star_brightness = 0;
                if (elapsed < 3000) {
                    star_brightness = (elapsed * 12) / 3000;
                } else if (elapsed < 13000) {
                    star_brightness = 12;
                } else if (elapsed < 15000) {
                    star_brightness = 12 - ((elapsed - 13000) * 12) / 2000;
                }

                // Time: fade in 2-4s, hold 4-10s, fade out 10-12s
                uint8_t time_brightness = 0;
                if (elapsed >= 2000 && elapsed < 4000) {
                    time_brightness = ((elapsed - 2000) * 15) / 2000;
                } else if (elapsed >= 4000 && elapsed < 10000) {
                    time_brightness = 15;
                } else if (elapsed >= 10000 && elapsed < 12000) {
                    time_brightness = 15 - ((elapsed - 10000) * 15) / 2000;
                }

                // Draw stars
                if (star_brightness > 0) {
                    star_field_draw(star_brightness, breath_phase);
                }

                // Draw time if bright enough
                if (time_brightness > 2) {
                    // Get current time
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    int hour = (tv.tv_sec % 86400) / 3600;
                    int min = (tv.tv_sec % 3600) / 60;
                    char time_str[8];
                    snprintf(time_str, sizeof(time_str), "%02d:%02d", hour, min);
                    lvgl_draw_text(128, 32, time_str, 28, time_brightness, true);
                }

                brightness = (star_brightness > time_brightness) ? star_brightness : time_brightness;
            }
            break;

        case SCREEN_WEATHER:
            {
                // Get weather data
                int hourly_count = 0;
                const hourly_data_t *hourly = weather_service_get_hourly(&hourly_count);

                // Calculate text brightness with breathing effect
                float breath = sinf(breath_phase * 0.5f) * 0.5f + 0.5f;
                uint8_t text_brightness = (uint8_t)((11 + breath * 2) * (brightness / 15.0f));
                if (text_brightness < 2) text_brightness = 2;
                if (text_brightness > 15) text_brightness = 15;

                if (hourly_count > 0) {
                    weather_chart_set_data(hourly, hourly_count);
                    weather_chart_draw(text_brightness, rain_anim_phase);
                }

                brightness = (brightness < 15) ? brightness + 1 : 15;
            }
            break;
    }

    // Trigger display refresh
    lvgl_trigger_refresh();
}
