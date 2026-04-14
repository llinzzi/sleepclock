#include "weather_service.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "WEATHER_SVC";

// QWeather API credentials
#define QWEATHER_HOST "nn3aaqw4wr.re.qweatherapi.com"
#define QWEATHER_KEY "e8e879aca230481f9201f67de0583184"
#define LOCATION_ID "101210101"

static weather_data_t current_weather;
static hourly_data_t hourly_forecast[24];
static int hourly_count = 0;
static bool data_valid = false;

static esp_err_t parse_hourly_forecast(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code == NULL || strcmp(code->valuestring, "200") != 0) {
        ESP_LOGE(TAG, "API error: %s", cJSON_GetObjectItem(root, "code") ? cJSON_GetObjectItem(root, "code")->valuestring : "unknown");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *hourly = cJSON_GetObjectItem(root, "hourly");
    if (hourly == NULL || !cJSON_IsArray(hourly)) {
        ESP_LOGE(TAG, "No hourly array in response");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    hourly_count = cJSON_GetArraySize(hourly);
    if (hourly_count > 24) hourly_count = 24;

    for (int i = 0; i < hourly_count; i++) {
        cJSON *item = cJSON_GetArrayItem(hourly, i);
        if (item == NULL) continue;

        cJSON *fxTime = cJSON_GetObjectItem(item, "fxTime");
        cJSON *temp = cJSON_GetObjectItem(item, "temp");
        cJSON *pop = cJSON_GetObjectItem(item, "pop");
        cJSON *precip = cJSON_GetObjectItem(item, "precip");
        cJSON *icon = cJSON_GetObjectItem(item, "icon");
        cJSON *text = cJSON_GetObjectItem(item, "text");

        // Parse hour from fxTime (ISO8601 format: 2026-04-14T18:00+08:00)
        int hour = 0;
        if (fxTime && cJSON_IsString(fxTime)) {
            const char *time_str = fxTime->valuestring;
            // Format: 2026-04-14T18:00+08:00
            // Extract hour from position 11-13
            if (strlen(time_str) >= 13) {
                hour = atoi(time_str + 11);
            }
        }

        hourly_forecast[i].hour = hour;
        hourly_forecast[i].temp = temp ? atoi(temp->valuestring) : 20;
        hourly_forecast[i].rain_prob = pop ? atoi(pop->valuestring) : 0;
        hourly_forecast[i].rain_mm = precip ? atof(precip->valuestring) : 0.0f;
        if (icon && cJSON_IsString(icon)) {
            snprintf(hourly_forecast[i].icon, sizeof(hourly_forecast[i].icon), "%s", icon->valuestring);
        }
    }

    // Update current weather from first hourly entry
    if (hourly_count > 0) {
        current_weather.temp = hourly_forecast[0].temp;
        current_weather.rain_prob = hourly_forecast[0].rain_prob;
        current_weather.rain_amount = hourly_forecast[0].rain_mm;
        if (hourly_forecast[0].icon[0]) {
            snprintf(current_weather.condition, sizeof(current_weather.condition), "%s", hourly_forecast[0].icon);
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t parse_daily_forecast(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        return ESP_FAIL;
    }

    cJSON *daily = cJSON_GetObjectItem(root, "daily");
    if (daily == NULL || !cJSON_IsArray(daily)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *today = cJSON_GetArrayItem(daily, 0);
    if (today == NULL) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *tempMax = cJSON_GetObjectItem(today, "tempMax");
    cJSON *tempMin = cJSON_GetObjectItem(today, "tempMin");

    current_weather.temp_max = tempMax ? atoi(tempMax->valuestring) : 30;
    current_weather.temp_min = tempMin ? atoi(tempMin->valuestring) : 20;

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t fetch_weather_data(const char *endpoint, esp_err_t (*parser)(const char *)) {
    char url[256];
    snprintf(url, sizeof(url), "https://%s%s?location=%s&key=%s",
             QWEATHER_HOST, endpoint, LOCATION_ID, QWEATHER_KEY);

    ESP_LOGI(TAG, "Fetching: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Invalid content length");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    char *buffer = malloc(content_length + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int read_len = esp_http_client_read(client, buffer, content_length);
    buffer[read_len] = 0;

    if (read_len <= 0) {
        ESP_LOGE(TAG, "Failed to read response");
        free(buffer);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Response: %.*s", read_len > 200 ? 200 : read_len, buffer);

    err = parser(buffer);

    free(buffer);
    esp_http_client_cleanup(client);

    return err;
}

esp_err_t weather_service_init(void) {
    ESP_LOGI(TAG, "Weather service init");
    memset(&current_weather, 0, sizeof(current_weather));
    memset(hourly_forecast, 0, sizeof(hourly_forecast));
    hourly_count = 0;
    data_valid = false;
    return ESP_OK;
}

esp_err_t weather_service_fetch(void) {
    ESP_LOGI(TAG, "Fetching weather data...");

    // Fetch hourly forecast
    esp_err_t err = fetch_weather_data("/v7/weather/24h", parse_hourly_forecast);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to fetch hourly forecast");
        return err;
    }

    // Fetch daily forecast
    err = fetch_weather_data("/v7/weather/7d", parse_daily_forecast);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to fetch daily forecast, using cached temp");
    }

    data_valid = true;
    ESP_LOGI(TAG, "Weather data updated successfully");
    return ESP_OK;
}

const hourly_data_t* weather_service_get_hourly(int *count) {
    *count = hourly_count;
    return hourly_forecast;
}

const weather_data_t* weather_service_get_current(void) {
    return &current_weather;
}
