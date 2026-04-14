#ifndef WEATHER_CHART_H
#define WEATHER_CHART_H

#include <stdint.h>

typedef struct {
    int hour;
    int temp;
    int rain_prob;
    float rain_mm;
    char icon[8];
} hourly_data_t;

void weather_chart_init(void);
void weather_chart_draw(uint8_t brightness, uint32_t rain_anim_phase);
void weather_chart_set_data(const hourly_data_t *data, int count);

#endif // WEATHER_CHART_H
