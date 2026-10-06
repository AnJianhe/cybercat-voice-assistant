#include "alarm_service.h"
#include "cloud_service.h"
#include "conversation_log.h"
#include "espnow_control.h"
#include "sr_engine.h"
#include "uart_link.h"
#include "wifi_service.h"
#include "wake_led.h"
#include "youdao_tts.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "赛博猫猫";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(youdao_tts_output_init());
    err = wake_led_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "状态灯不可用");
    err = conversation_log_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "对话记录存储不可用");
    ESP_ERROR_CHECK(uart_link_init());
    ESP_ERROR_CHECK(wifi_service_init());

    if (!wifi_service_wait_connected(10000)) {
        ESP_LOGW(TAG, "Wi‑Fi 暂未连接；离线功能可用，后台会自动重连");
    }
    err = espnow_control_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "设备控制通道不可用");
    ESP_ERROR_CHECK(sr_engine_init());
    ESP_ERROR_CHECK(cloud_service_init());
    ESP_ERROR_CHECK(alarm_service_init());
    ESP_LOGI(TAG, "赛博猫猫已启动：S3 为大脑，音频桥为嘴巴，C3 为手脚");
}
