#include "espnow_control.h"
#include "app_config.h"
#include "wifi_service.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

typedef struct { char text[24]; } control_message_t;
static const char *TAG = "设备控制";
static QueueHandle_t s_queue;
static const uint8_t s_peer[6] = {C3_MAC_0, C3_MAC_1, C3_MAC_2, C3_MAC_3, C3_MAC_4, C3_MAC_5};

static bool mac_is_configured(void)
{
    uint8_t any = 0;
    for (size_t i = 0; i < sizeof(s_peer); ++i) any |= s_peer[i];
    return any != 0;
}

static void esp_now_task(void *arg)
{
    (void)arg;
    control_message_t message;
    for (;;) {
        if (xQueueReceive(s_queue, &message, portMAX_DELAY) == pdTRUE) {
            esp_err_t err = esp_now_send(s_peer, (const uint8_t *)message.text, strlen(message.text) + 1);
            if (err != ESP_OK) ESP_LOGE(TAG, "发送设备指令失败：%s", message.text);
        }
    }
}

esp_err_t espnow_control_init(void)
{
    if (!mac_is_configured()) {
        ESP_LOGW(TAG, "C3 地址尚未配置；家电控制暂不可用");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init");
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_peer, sizeof(s_peer));
    peer.ifidx = WIFI_IF_STA;
    peer.channel = 0; /* 0 means the current STA channel; it tracks the AP channel. */
    peer.encrypt = false;
    ESP_RETURN_ON_ERROR(esp_now_add_peer(&peer), TAG, "add peer");
    ESP_LOGI(TAG, "控制从机已就绪，Wi‑Fi 信道=%d", wifi_service_channel());
    s_queue = xQueueCreate(8, sizeof(control_message_t));
    if (!s_queue) return ESP_ERR_NO_MEM;
    return xTaskCreate(esp_now_task, "esp_now_task", 3072, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t espnow_control_send(const char *command)
{
    if (!s_queue || !command) return ESP_ERR_INVALID_STATE;
    control_message_t message = {0};
    if (strlcpy(message.text, command, sizeof(message.text)) >= sizeof(message.text)) return ESP_ERR_INVALID_SIZE;
    return xQueueSend(s_queue, &message, pdMS_TO_TICKS(50)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
