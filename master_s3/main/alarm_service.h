#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t repeat_mask; /* bit0=Sunday ... bit6=Saturday */
    uint8_t volume;
    bool enabled;
    char music[96];
} alarm_config_t;

esp_err_t alarm_service_init(void);
esp_err_t alarm_service_set(const alarm_config_t *config);
esp_err_t alarm_service_get(alarm_config_t *config);

