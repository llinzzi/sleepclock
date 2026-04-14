#include "star_field.h"
#include "lvgl_adapter.h"
#include <stdlib.h>
#include <math.h>

#define STAR_COUNT 30

static star_t stars[STAR_COUNT];
static bool initialized = false;

void star_field_init(void) {
    if (initialized) return;

    // Use fixed seed for consistent star positions
    srand(12345);
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x = rand() % 256;
        stars[i].y = rand() % 64;
        stars[i].size = (rand() % 100) < 30 ? 2 : 1;
        stars[i].brightness = 2 + (rand() % 4); // 2-5
    }
    initialized = true;
}

void star_field_draw(uint8_t brightness, float breath_phase) {
    if (brightness == 0) return;

    for (int i = 0; i < STAR_COUNT; i++) {
        star_t *s = &stars[i];
        // Flicker effect: sin(breath_phase * 3 + x) * 0.5 + 0.5
        float flicker = sinf(breath_phase * 3.0f + s->x) * 0.5f + 0.5f;
        uint8_t b = (uint8_t)((float)s->brightness * flicker * (brightness / 15.0f));
        if (b < 2) b = 2;
        if (b > 15) b = 15;

        // Draw pixel(s)
        if (s->size == 1) {
            lvgl_draw_pixel(s->x, s->y, b);
        } else {
            lvgl_draw_pixel(s->x, s->y, b);
            lvgl_draw_pixel(s->x + 1, s->y, b);
        }
    }
}
