#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include "weather_chart.h"
#include "esp_err.h"

typedef struct {
    int temp;
    int rain_prob;
    float rain_amount;
    char condition[16];
    int temp_max;
    int temp_min;
} weather_data_t;

esp_err_t weather_service_init(void);
esp_err_t weather_service_fetch(void);
const hourly_data_t* weather_service_get_hourly(int *count);
const weather_data_t* weather_service_get_current(void);

#endif // WEATHER_SERVICE_H
