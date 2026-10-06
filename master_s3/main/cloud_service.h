#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t cloud_service_init(void);
esp_err_t cloud_service_submit_prompt(const char *prompt);
esp_err_t cloud_service_submit_tts_test(const char *text);
esp_err_t cloud_service_set_api_key(const char *key);
bool cloud_service_usb_command(const char *command);
