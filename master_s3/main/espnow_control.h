#pragma once
#include "esp_err.h"

esp_err_t espnow_control_init(void);
esp_err_t espnow_control_send(const char *command);

