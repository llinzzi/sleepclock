#include "weather_chart.h"
#include "lvgl_adapter.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_HOURLY 24

static hourly_data_t hourly_data[MAX_HOURLY];
static int hourly_count = 0;

void weather_chart_init(void) {
    hourly_count = 0;
}

void weather_chart_set_data(const hourly_data_t *data, int count) {
    if (count > MAX_HOURLY) count = MAX_HOURLY;
    hourly_count = count;
    for (int i = 0; i < count; i++) {
        hourly_data[i] = data[i];
    }
}

void weather_chart_draw(uint8_t brightness, uint32_t rain_anim_phase) {
    if (hourly_count == 0 || brightness < 2) return;

    // Only show first 12 hours
    int count = hourly_count < 12 ? hourly_count : 12;

    // Calculate temperature range for Y mapping
    int temp_min = 100, temp_max = -100;
    for (int i = 0; i < count; i++) {
        if (hourly_data[i].temp < temp_min) temp_min = hourly_data[i].temp;
        if (hourly_data[i].temp > temp_max) temp_max = hourly_data[i].temp;
    }
    if (temp_max == temp_min) temp_max = temp_min + 1;

    // Chart bounds
    int chart_top = 8;
    int chart_bottom = 45;
    int chart_height = chart_bottom - chart_top;

    // Calculate X positions for each hour
    int x_positions[12];
    for (int i = 0; i < count; i++) {
        x_positions[i] = 14 + i * 20;
    }

    // Draw rain animation for hours with rain_prob >= 30
    for (int i = 0; i < count; i++) {
        if (hourly_data[i].rain_prob >= 30) {
            int x = x_positions[i];
            // Rain speed based on probability
            int rain_speed = 10 + hourly_data[i].rain_prob / 5;
            // Drop count based on probability
            int drop_count = hourly_data[i].rain_prob / 10;
            if (drop_count < 1) drop_count = 1;
            if (drop_count > 10) drop_count = 10;

            // Drop spacing based on probability
            int drop_spacing = 20 - hourly_data[i].rain_prob / 10;
            if (drop_spacing < 8) drop_spacing = 8;

            // Draw falling rain drops
            int anim_y = (rain_anim_phase * rain_speed / 10) % 50;
            for (int d = 0; d < drop_count; d++) {
                int drop_y = (anim_y + d * drop_spacing) % 50;
                // Rain drop is 1x4 pixels
                lvgl_draw_pixel(x + d * 3, drop_y + 5, brightness - 5);
            }
        }
    }

    // Draw temperature curve (line connecting all points)
    for (int i = 0; i < count - 1; i++) {
        int x1 = x_positions[i];
        int x2 = x_positions[i + 1];
        int y1 = chart_bottom - ((hourly_data[i].temp - temp_min) * chart_height) / (temp_max - temp_min);
        int y2 = chart_bottom - ((hourly_data[i + 1].temp - temp_min) * chart_height) / (temp_max - temp_min);
        lvgl_draw_line(x1, y1, x2, y2, brightness);
    }

    // Draw dots and temp labels
    for (int i = 0; i < count; i++) {
        int x = x_positions[i];
        int y = chart_bottom - ((hourly_data[i].temp - temp_min) * chart_height) / (temp_max - temp_min);

        // Draw dot
        lvgl_draw_circle(x, y, 2, brightness + 2);

        // Temp label - always above dot, min y=10
        char temp_str[8];
        snprintf(temp_str, sizeof(temp_str), "%d°", hourly_data[i].temp);
        int label_y = y - 6;
        if (label_y < 10) label_y = 10;
        lvgl_draw_text(x, label_y, temp_str, 7, brightness - 2, true);
    }

    // Draw time axis line
    lvgl_draw_line(8, 48, 248, 48, brightness - 6);

    // Draw hour labels
    for (int i = 0; i < count; i++) {
        int x = x_positions[i];
        char hour_str[8];
        if (i == 0) {
            // First hour shows H:MM format (current time)
            snprintf(hour_str, sizeof(hour_str), "%d:00", hourly_data[i].hour);
            lvgl_draw_text(x, 56, hour_str, 7, brightness, true);
        } else {
            snprintf(hour_str, sizeof(hour_str), "%d", hourly_data[i].hour);
            lvgl_draw_text(x, 56, hour_str, 6, brightness - 5, false);
        }
    }
}
