#include "alarm_service.h"
#include "app_config.h"
#include "cloud_service.h"
#include "uart_link.h"
#include "wifi_service.h"

#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "闹钟";
static alarm_config_t s_alarm;
static bool s_triggered_this_minute;

esp_err_t alarm_service_set(const alarm_config_t *config)
{
    if (!config || config->hour > 23 || config->minute > 59 || config->volume > 100 || !config->music[0])
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("alarm", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(nvs, "config", config, sizeof(*config));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err == ESP_OK) s_alarm = *config;
    return err;
}

esp_err_t alarm_service_get(alarm_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    *config = s_alarm;
    return ESP_OK;
}

static void alarm_task(void *arg)
{
    (void)arg;
    for (;;) {
        time_t now = time(NULL);
        struct tm local;
        localtime_r(&now, &local);
        bool valid_time = local.tm_year + 1900 >= 2024;
        bool match = valid_time && s_alarm.enabled && (s_alarm.repeat_mask & (1U << local.tm_wday)) &&
                     local.tm_hour == s_alarm.hour && local.tm_min == s_alarm.minute;
        if (match && !s_triggered_this_minute) {
            char command[128];
            snprintf(command, sizeof(command), "VOL:%u", s_alarm.volume);
            uart_link_command(command);
            snprintf(command, sizeof(command), "PLAY:%s", s_alarm.music);
            uart_link_command(command); /* Local fallback starts regardless of Internet state. */
            if (wifi_service_wait_connected(50)) {
                cloud_service_submit_prompt("现在是起床时间。请生成一句不超过40字、温暖又有猫咪风格的中文叫醒语。");
            }
            s_triggered_this_minute = true;
            ESP_LOGI(TAG, "闹钟已触发");
        }
        if (!match) s_triggered_this_minute = false;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t alarm_service_init(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("alarm", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    size_t size = sizeof(s_alarm);
    err = nvs_get_blob(nvs, "config", &s_alarm, &size);
    nvs_close(nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND || size != sizeof(s_alarm)) {
        alarm_config_t defaults = {
            .hour = DEFAULT_ALARM_HOUR,
            .minute = DEFAULT_ALARM_MINUTE,
            .repeat_mask = DEFAULT_ALARM_REPEAT_MASK,
            .volume = DEFAULT_ALARM_VOLUME,
            .enabled = false,
        };
        strlcpy(defaults.music, DEFAULT_ALARM_MUSIC, sizeof(defaults.music));
        ESP_RETURN_ON_ERROR(alarm_service_set(&defaults), TAG, "store defaults");
    } else if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(alarm_task, "alarm_task", 4096, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
