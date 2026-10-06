#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_service_init(void);
bool wifi_service_wait_connected(unsigned timeout_ms);
int wifi_service_channel(void);
esp_err_t wifi_service_set_credentials(const char *ssid, const char *password);
void wifi_service_scan_usb(void);
const char *wifi_service_configured_ssid(void);
