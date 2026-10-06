#include "uart_link.h"
#include "app_config.h"
#include "cloud_service.h"
#include "youdao_tts.h"

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "音频链路";
static SemaphoreHandle_t s_tx_lock;
static EventGroupHandle_t s_events;
/* PCM 发送用滑动窗口：链路自测证明连续发送可达 ~85 KB/s，而「发一块等一块」
   只有 ~25 KB/s（每帧要等驱动/中断重新起步约 17 ms）。窗口内允许 4 块在途，
   Bridge 依然逐块确认，因此背压与溢出保护都还在。 */
#define PCM_ACK_WINDOW 1
static volatile int s_pcm_inflight = 0;
static volatile bool s_pcm_error = false;
static volatile bool s_pcm_crc = false;

#define EVENT_TTS_STARTED  BIT0
#define EVENT_TTS_FINISHED BIT1
#define EVENT_TTS_ERROR    BIT2
#define EVENT_MP3_STARTED  BIT3
#define EVENT_MP3_DATA     BIT4
#define EVENT_MP3_FINISHED BIT5
#define EVENT_MP3_ERROR    BIT6
#define EVENT_PCM_STARTED  BIT7
#define EVENT_PCM_DATA     BIT8
#define EVENT_PCM_FINISHED BIT9
#define EVENT_PCM_ERROR    BIT10
#define EVENT_PCM_ALL      (EVENT_PCM_STARTED | EVENT_PCM_DATA | EVENT_PCM_FINISHED | EVENT_PCM_ERROR)

static void uart_rx_task(void *arg)
{
    (void)arg;
    frame_parser_t parser;
    frame_parser_init(&parser);
    uint8_t data[256];
    for (;;) {
        /* 超时给 1 个 tick（10 ms）。注意 pdMS_TO_TICKS(2) 在 100 Hz tick 下等于 0，
           会让这个高优先级任务变成非阻塞轮询，把同核 CPU 和 IDLE 一起饿死（触发看门狗）。 */
        int n = uart_read_bytes(BRIDGE_UART_NUM, data, sizeof(data), pdMS_TO_TICKS(10));
        for (int i = 0; i < n; ++i) {
            frame_t frame;
            bool crc_error = false;
            if (frame_parser_push(&parser, data[i], &frame, &crc_error)) {
                if (frame.type == FRAME_CMD) {
                    char command[FRAME_MAX_PAYLOAD + 1];
                    memcpy(command, frame.payload, frame.length);
                    command[frame.length] = 0;
                    const char prefix[] = "SET_DEEPSEEK_KEY:";
                    if (strncmp(command, prefix, sizeof(prefix) - 1) == 0) {
                        esp_err_t err = cloud_service_set_api_key(command + sizeof(prefix) - 1);
                        memset(command, 0, sizeof(command));
                        const char *reply = err == ESP_OK ? "OK:DEEPSEEK_KEY_SAVED" : "ERR:DEEPSEEK_KEY";
                        uart_link_send(err == ESP_OK ? FRAME_ACK : FRAME_ERROR,
                                       reply, (uint16_t)strlen(reply));
                    } else if (strncmp(command, "TTS_TEST:", 9) == 0) {
                        esp_err_t err = cloud_service_submit_tts_test(command + 9);
                        memset(command, 0, sizeof(command));
                        const char *reply = err == ESP_OK ? "OK:TTS_TEST_QUEUED" : "ERR:TTS_TEST";
                        uart_link_send(err == ESP_OK ? FRAME_ACK : FRAME_ERROR,
                                       reply, (uint16_t)strlen(reply));
                    } else {
                        memset(command, 0, sizeof(command));
                        const char *reply = "ERR:UNKNOWN_S3_COMMAND";
                        uart_link_send(FRAME_ERROR, reply, (uint16_t)strlen(reply));
                    }
                } else if (frame.type == FRAME_ACK || frame.type == FRAME_ERROR) {
                    char text[FRAME_MAX_PAYLOAD + 1];
                    memcpy(text, frame.payload, frame.length);
                    text[frame.length] = 0;
                    /* 逐块确认是音频路径上的高频事件：控制台写入会阻塞任务
                       （实测单条日志 137 ms），从而把整个推流拖慢 6 倍以上。
                       这类 ACK 只在出错时打印，正常流不再刷屏。 */
                    const bool per_chunk = strcmp(text, "OK:PCM_DATA") == 0 ||
                                           strcmp(text, "OK:MP3_DATA") == 0;
                    if (frame.type == FRAME_ERROR || !per_chunk) {
                        ESP_LOGI(TAG, "音频桥%s：%s",
                                 frame.type == FRAME_ACK ? "ACK" : "ERROR", text);
                    }
                    if (s_events) {
                        if (frame.type == FRAME_ERROR) {
                            if (strcmp(text, "ERR:CRC") == 0) {
                                /* 单块损坏不是致命错误：Bridge 已丢弃该帧并重新同步，
                                   这里只把该块标为需重发，避免整段语音中断。 */
                                if (s_pcm_inflight > 0) --s_pcm_inflight;
                                s_pcm_crc = true;
                                ESP_LOGW(TAG, "PCM 块 CRC 错误，准备重发");
                            } else {
                                xEventGroupSetBits(s_events,
                                                   EVENT_TTS_ERROR | EVENT_MP3_ERROR | EVENT_PCM_ERROR);
                                s_pcm_error = true;
                            }
                        } else if (strcmp(text, "OK:TTS_STARTED") == 0) {
                            xEventGroupSetBits(s_events, EVENT_TTS_STARTED);
                        } else if (strcmp(text, "OK:TTS_ENDED") == 0) {
                            xEventGroupSetBits(s_events, EVENT_TTS_FINISHED);
                        } else if (strcmp(text, "OK:MP3_STARTED") == 0) {
                            xEventGroupSetBits(s_events, EVENT_MP3_STARTED);
                        } else if (strcmp(text, "OK:MP3_DATA") == 0) {
                            xEventGroupSetBits(s_events, EVENT_MP3_DATA);
                        } else if (strcmp(text, "OK:MP3_ENDED") == 0) {
                            xEventGroupSetBits(s_events, EVENT_MP3_FINISHED);
                        } else if (strcmp(text, "OK:PCM_STARTED") == 0) {
                            xEventGroupSetBits(s_events, EVENT_PCM_STARTED);
                        } else if (strcmp(text, "OK:PCM_DATA") == 0 ||
                                   strcmp(text, "OK:PCM_DROPPED") == 0) {
                            /* PCM_DROPPED：Bridge 环满丢了这一块，算作已确认，继续往下发。 */
                            if (s_pcm_inflight > 0) --s_pcm_inflight;
                            xEventGroupSetBits(s_events, EVENT_PCM_DATA);
                        } else if (strcmp(text, "OK:PCM_ENDED") == 0) {
                            xEventGroupSetBits(s_events, EVENT_PCM_FINISHED);
                        }
                    }
                }
            } else if (crc_error) {
                ESP_LOGW(TAG, "已丢弃校验失败的音频桥数据帧");
            }
        }
    }
}

esp_err_t uart_link_init(void)
{
    const uart_config_t config = {
        .baud_rate = BRIDGE_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(BRIDGE_UART_NUM, 8192, 8192, 0, NULL, 0), TAG, "driver");
    ESP_RETURN_ON_ERROR(uart_param_config(BRIDGE_UART_NUM, &config), TAG, "config");
    ESP_RETURN_ON_ERROR(uart_set_pin(BRIDGE_UART_NUM, BRIDGE_UART_TX_GPIO, BRIDGE_UART_RX_GPIO,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "pins");
    s_tx_lock = xSemaphoreCreateMutex();
    s_events = xEventGroupCreate();
    if (!s_tx_lock || !s_events) return ESP_ERR_NO_MEM;
    /* Parser + completed frame + printable ACK can coexist on this task's stack. */
    if (xTaskCreate(uart_rx_task, "uart_rx_task", 10240, NULL, 7, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}

esp_err_t uart_link_send(frame_type_t type, const void *payload, uint16_t length)
{
    if (!s_tx_lock || length > FRAME_MAX_PAYLOAD) return ESP_ERR_INVALID_STATE;
    uint8_t encoded[FRAME_MAX_PAYLOAD + 7];
    size_t n = frame_encode(type, payload, length, encoded, sizeof(encoded));
    if (!n) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_tx_lock, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    int written = uart_write_bytes(BRIDGE_UART_NUM, encoded, n);
    if (written == (int)n) uart_wait_tx_done(BRIDGE_UART_NUM, pdMS_TO_TICKS(1000));
    xSemaphoreGive(s_tx_lock);
    return written == (int)n ? ESP_OK : ESP_FAIL;
}

esp_err_t uart_link_command(const char *command)
{
    if (!command) return ESP_ERR_INVALID_ARG;
    size_t n = strlen(command);
    return n <= FRAME_MAX_PAYLOAD ? uart_link_send(FRAME_CMD, command, (uint16_t)n) : ESP_ERR_INVALID_SIZE;
}

esp_err_t uart_link_stop(void)
{
    youdao_tts_cancel();
    return youdao_tts_speaker_mode() ? ESP_OK : uart_link_send(FRAME_STOP, NULL, 0);
}

/* TTS 走 PCM 流：每块都要等 Bridge 确认，发送速率因此被锁到 A2DP 的真实消费速率，
   不会把 4 KB 环形缓冲冲爆。 */
esp_err_t uart_link_pcm_start(uint32_t rate, uint8_t bits, uint8_t channels)
{
    if (!s_events) return ESP_ERR_INVALID_STATE;
    s_pcm_inflight = 0;
    s_pcm_error = false;
    uint8_t p[7] = {(uint8_t)(rate >> 24), (uint8_t)(rate >> 16), (uint8_t)(rate >> 8),
                    (uint8_t)rate, bits, channels, 0};
    for (int attempt = 0; attempt < 3; ++attempt) {
        xEventGroupClearBits(s_events, EVENT_PCM_ALL);
        esp_err_t err = uart_link_send(FRAME_PCM_START, p, sizeof(p));
        if (err != ESP_OK) continue;
        EventBits_t bits_set = xEventGroupWaitBits(
            s_events, EVENT_PCM_STARTED | EVENT_PCM_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
        if (bits_set & EVENT_PCM_STARTED) return ESP_OK;
        if (bits_set & EVENT_PCM_ERROR) return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t uart_link_pcm_data(const uint8_t *data, size_t length)
{
    if (!data || !length || length > UART_PCM_CHUNK_BYTES || !s_events) return ESP_ERR_INVALID_ARG;
    for (int attempt = 0; attempt < 4; ++attempt) {
        /* 窗口满则等一个确认；Bridge 只有在环形缓冲有空间时才确认，所以这就是流控。 */
        int64_t deadline = esp_timer_get_time() + 5000000;
        while (s_pcm_inflight >= PCM_ACK_WINDOW) {
            if (s_pcm_error) return ESP_FAIL;
            if (esp_timer_get_time() > deadline) {
                /* 上一步确认丢了也继续往下走：宁可这一块缺口，也不要丢掉整句。 */
                s_pcm_inflight = 0;
                break;
            }
            vTaskDelay(1);
        }
        if (s_pcm_error) return ESP_FAIL;
        /* 音频路径上绝不写日志：S3 的 USB 控制台在主机读得慢时会阻塞上百毫秒，
           足以卡死接收任务的 ACK 处理，让整句语音超时失败（实测复现）。 */
        s_pcm_crc = false;
        /* 先登记在途，再发：ACK 可能在 uart_link_send 内部（它要等 TX 完成）就回来，
           否则会把没登记的确认吃掉，计数永远卡住。 */
        ++s_pcm_inflight;
        esp_err_t err = uart_link_send(FRAME_PCM_DATA, data, (uint16_t)length);
        if (err != ESP_OK) {
            if (s_pcm_inflight > 0) --s_pcm_inflight;
            return err;
        }
        /* 等这一块的结果：正常确认就返回，收到 CRC 通知则重发同一块。 */
        deadline = esp_timer_get_time() + 5000000;
        while (s_pcm_inflight > 0) {
            if (s_pcm_error) return ESP_FAIL;
            if (s_pcm_crc) break;
            if (esp_timer_get_time() > deadline) {
                /* 对端偶发停顿（蓝牙占用、驱动打嗝）时不要放弃整句语音：
                   把这一块当作丢失并重发。 */
                if (s_pcm_inflight > 0) --s_pcm_inflight;
                s_pcm_crc = true;
                break;
            }
            vTaskDelay(1);
        }
        if (!s_pcm_crc) return ESP_OK;
    }
    ESP_LOGE(TAG, "PCM 块连续重发失败");
    return ESP_FAIL;
}

esp_err_t uart_link_pcm_end(void)
{
    /* 先等在途数据全部被确认，再发结束帧，避免尾部音频被丢弃。 */
    int64_t deadline = esp_timer_get_time() + 5000000;
    while (s_pcm_inflight > 0) {
        if (s_pcm_error) return ESP_FAIL;
        if (esp_timer_get_time() > deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(1);
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        xEventGroupClearBits(s_events, EVENT_PCM_ALL);
        esp_err_t err = uart_link_send(FRAME_PCM_END, NULL, 0);
        if (err != ESP_OK) continue;
        EventBits_t bits_set = xEventGroupWaitBits(
            s_events, EVENT_PCM_FINISHED | EVENT_PCM_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
        if (bits_set & EVENT_PCM_FINISHED) return ESP_OK;
        if (bits_set & EVENT_PCM_ERROR) return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t uart_link_tts_text(const char *text)
{
    if (!text || !*text || !s_events) return ESP_ERR_INVALID_ARG;
    size_t length = strlen(text);
    if (length > FRAME_MAX_PAYLOAD) {
        length = FRAME_MAX_PAYLOAD;
        /* Never split a UTF-8 code point at the payload boundary. */
        while (length && (((uint8_t)text[length] & 0xc0) == 0x80)) --length;
        ESP_LOGW(TAG, "语音合成文字过长，已截断至 %u 字节", (unsigned)length);
    }
    xEventGroupClearBits(s_events, EVENT_TTS_STARTED | EVENT_TTS_FINISHED | EVENT_TTS_ERROR);
    esp_err_t err = uart_link_send(FRAME_TTS_TEXT, text, (uint16_t)length);
    if (err != ESP_OK) return err;
    EventBits_t bits = xEventGroupWaitBits(
        s_events, EVENT_TTS_STARTED | EVENT_TTS_ERROR, pdTRUE, pdFALSE,
        pdMS_TO_TICKS(35000));
    if (bits & EVENT_TTS_ERROR) return ESP_FAIL;
    if (!(bits & EVENT_TTS_STARTED)) {
        ESP_LOGE(TAG, "等待音频桥开始语音合成超时");
        return ESP_ERR_TIMEOUT;
    }
    bits = xEventGroupWaitBits(
        s_events, EVENT_TTS_FINISHED | EVENT_TTS_ERROR, pdTRUE, pdFALSE,
        pdMS_TO_TICKS(90000));
    if (bits & EVENT_TTS_ERROR) return ESP_FAIL;
    if (!(bits & EVENT_TTS_FINISHED)) {
        ESP_LOGE(TAG, "等待音频桥完成语音合成超时");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t uart_link_mp3_start(uint32_t total_bytes)
{
    uint8_t header[4] = {
        (uint8_t)(total_bytes >> 24), (uint8_t)(total_bytes >> 16),
        (uint8_t)(total_bytes >> 8), (uint8_t)total_bytes,
    };
    for (int attempt = 0; attempt < 3; ++attempt) {
        xEventGroupClearBits(s_events, EVENT_MP3_STARTED | EVENT_MP3_ERROR);
        esp_err_t err = uart_link_send(FRAME_MP3_START, header, sizeof(header));
        if (err != ESP_OK) continue;
        EventBits_t bits = xEventGroupWaitBits(s_events, EVENT_MP3_STARTED | EVENT_MP3_ERROR,
                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
        if (bits & EVENT_MP3_STARTED) return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t uart_link_mp3_data(const uint8_t *data, size_t length)
{
    if (!data || !length || length > FRAME_MAX_PAYLOAD) return ESP_ERR_INVALID_ARG;
    for (int attempt = 0; attempt < 3; ++attempt) {
        xEventGroupClearBits(s_events, EVENT_MP3_DATA | EVENT_MP3_ERROR);
        esp_err_t err = uart_link_send(FRAME_MP3_DATA, data, (uint16_t)length);
        if (err != ESP_OK) continue;
        EventBits_t bits = xEventGroupWaitBits(s_events, EVENT_MP3_DATA | EVENT_MP3_ERROR,
                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
        if (bits & EVENT_MP3_DATA) return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t uart_link_mp3_end(void)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        xEventGroupClearBits(s_events, EVENT_MP3_FINISHED | EVENT_MP3_ERROR);
        esp_err_t err = uart_link_send(FRAME_MP3_END, NULL, 0);
        if (err != ESP_OK) continue;
        EventBits_t bits = xEventGroupWaitBits(s_events, EVENT_MP3_FINISHED | EVENT_MP3_ERROR,
                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(3000));
        if (bits & EVENT_MP3_FINISHED) return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t uart_link_status(const char *status)
{
    if (!status || !*status) return ESP_ERR_INVALID_ARG;
    size_t length = strlen(status);
    return length <= FRAME_MAX_PAYLOAD ?
        uart_link_send(FRAME_STATUS, status, (uint16_t)length) : ESP_ERR_INVALID_SIZE;
}

/* 诊断用：连续猛发 N 个 1024 字节帧，测链路真实吞吐。
   帧类型故意用接收端不认识的值，Bridge 会静默忽略，不产生回程流量。 */
void uart_link_selftest(int chunks)
{
    if (chunks <= 0) return;
    static uint8_t payload[1024];
    for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = (uint8_t)i;
    const uint8_t probe_type = 0x0D;
    int64_t started = esp_timer_get_time();
    for (int i = 0; i < chunks; ++i) {
        frame_type_t type = (frame_type_t)probe_type;
        uart_link_send(type, payload, sizeof(payload));
    }
    uart_wait_tx_done(BRIDGE_UART_NUM, pdMS_TO_TICKS(2000));
    int64_t elapsed_us = esp_timer_get_time() - started;
    size_t total = (size_t)chunks * sizeof(payload);
    ESP_LOGI(TAG, "链路自测：%d 块 %u 字节，耗时 %d ms，%.1f KB/s",
             chunks, (unsigned)total, (int)(elapsed_us / 1000),
             elapsed_us > 0 ? (double)total * 1000.0 / (double)elapsed_us : 0.0);
}
