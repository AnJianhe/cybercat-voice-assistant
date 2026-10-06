#include "wifi_service.h"
#include "app_config.h"

#include "esp_event.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIFI_CONNECTED_BIT BIT0
static const char *TAG = "网络";
static EventGroupHandle_t s_events;
static char s_ssid[33];
static char s_password[65];

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "网络已断开，正在重连");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_service_init(void)
{
    s_events = xEventGroupCreate();
    if (!s_events) return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init");
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
    strlcpy(s_ssid, WIFI_SSID, sizeof(s_ssid));
    strlcpy(s_password, WIFI_PASSWORD, sizeof(s_password));
    nvs_handle_t handle;
    if (nvs_open("cybercat", NVS_READONLY, &handle) == ESP_OK) {
        size_t ssid_len = sizeof(s_ssid), pass_len = sizeof(s_password);
        nvs_get_str(handle, "wifi_ssid", s_ssid, &ssid_len);
        nvs_get_str(handle, "wifi_pass", s_password, &pass_len);
        nvs_close(handle);
    }
    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, s_ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, s_password, sizeof(config.sta.password));
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());

    setenv("TZ", SYSTEM_TIMEZONE, 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER_1);
    esp_sntp_setservername(1, NTP_SERVER_2);
    esp_sntp_init();
    return ESP_OK;
}

bool wifi_service_wait_connected(unsigned timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(s_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                                            pdMS_TO_TICKS(timeout_ms));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

int wifi_service_channel(void)
{
    uint8_t primary = 0;
    wifi_second_chan_t secondary;
    return esp_wifi_get_channel(&primary, &secondary) == ESP_OK ? primary : 0;
}

const char *wifi_service_configured_ssid(void)
{
    return s_ssid;
}

esp_err_t wifi_service_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !*ssid || !password || strlen(ssid) > 32 || strlen(password) > 64) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open("cybercat", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, "wifi_ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, "wifi_pass", password);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return err;
    strlcpy(s_ssid, ssid, sizeof(s_ssid));
    strlcpy(s_password, password, sizeof(s_password));
    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, s_ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, s_password, sizeof(config.sta.password));
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    (void)esp_wifi_disconnect();
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &config), TAG, "set config");
    return esp_wifi_connect();
}

void wifi_service_scan_usb(void)
{
    wifi_scan_config_t config = {0};
    printf("@S3 WIFI_BEGIN\n");
    esp_err_t err = esp_wifi_scan_start(&config, true);
    if (err != ESP_OK) {
        printf("@S3 WIFI_ERROR %s\n", esp_err_to_name(err));
        fflush(stdout);
        return;
    }
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count > 32) count = 32;
    wifi_ap_record_t records[32];
    uint16_t fetched = count;
    if (fetched && esp_wifi_scan_get_ap_records(&fetched, records) == ESP_OK) {
        static const char hex[] = "0123456789abcdef";
        for (uint16_t i = 0; i < fetched; ++i) {
            char encoded[65];
            size_t length = strnlen((const char *)records[i].ssid, 32);
            for (size_t j = 0; j < length; ++j) {
                encoded[j * 2] = hex[records[i].ssid[j] >> 4];
                encoded[j * 2 + 1] = hex[records[i].ssid[j] & 0x0f];
            }
            encoded[length * 2] = 0;
            printf("@S3 WIFI %s %d %d\n", encoded, records[i].rssi,
                   records[i].authmode != WIFI_AUTH_OPEN);
        }
    }
    printf("@S3 WIFI_END %u\n", (unsigned)fetched);
    fflush(stdout);
}
