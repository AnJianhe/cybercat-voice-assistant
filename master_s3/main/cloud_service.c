#include "cloud_service.h"
#include "conversation_log.h"
#include "app_config.h"
#include "sr_engine.h"
#include "uart_link.h"
#include "wake_led.h"
#include "wifi_service.h"
#include "youdao_asr.h"
#include "youdao_tts.h"

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "nvs.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char text[512]; bool tts_only; } prompt_item_t;
static const char *TAG = "云端";
static QueueHandle_t s_prompts;
static EventGroupHandle_t s_pc_tts_events;
static SemaphoreHandle_t s_api_key_lock;
static char s_deepseek_api_key[160];

#define PC_TTS_STARTED  BIT0
#define PC_TTS_FINISHED BIT1
#define PC_TTS_ERROR    BIT2

static int64_t monotonic_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/* One concise record per conversation phase.  Do not log from the PCM loop:
 * USB console back-pressure there can break the UART audio flow-control. */
static void log_latency(const conversation_stamp_t *stamp, const char *phase, int64_t began_ms)
{
    int64_t now = monotonic_ms();
    if (stamp) {
        ESP_LOGI(TAG, "延时 %s=%lld ms，总计=%lld ms", phase,
                 (long long)(now - began_ms), (long long)(now - stamp->started_ms));
    } else {
        ESP_LOGI(TAG, "延时 %s=%lld ms", phase, (long long)(now - began_ms));
    }
}

esp_err_t cloud_service_set_api_key(const char *key)
{
    if (!key || !s_api_key_lock) return ESP_ERR_INVALID_STATE;
    size_t length = strlen(key);
    if (length < 16 || length >= sizeof(s_deepseek_api_key)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("cybercat", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, "deepseek", key);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return err;
    if (xSemaphoreTake(s_api_key_lock, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    strlcpy(s_deepseek_api_key, key, sizeof(s_deepseek_api_key));
    xSemaphoreGive(s_api_key_lock);
    ESP_LOGI(TAG, "DeepSeek 密钥已安全保存到本机配置");
    return ESP_OK;
}


bool cloud_service_usb_command(const char *command)
{
#if PC_TTS_OVER_USB
    if (!command || !s_pc_tts_events) return false;
    EventBits_t bit = 0;
    if (strcmp(command, "PC_TTS_STARTED") == 0) bit = PC_TTS_STARTED;
    else if (strcmp(command, "PC_TTS_ENDED") == 0) bit = PC_TTS_FINISHED;
    else if (strcmp(command, "PC_TTS_ERROR") == 0) bit = PC_TTS_ERROR;
    else return false;
    xEventGroupSetBits(s_pc_tts_events, bit);
    return true;
#else
    (void)command;
    return false;
#endif
}


#if PC_TTS_OVER_USB
static void emit_pc_tts_text(const char *text)
{
    const unsigned char *raw = (const unsigned char *)text;
    size_t raw_bytes = strlen(text);
    unsigned char encoded[257]; /* 192 raw bytes -> 256 base64 bytes + NUL */
    printf("PC_TTS_TEXT_BEGIN:%u\n", (unsigned)raw_bytes);
    for (size_t offset = 0; offset < raw_bytes; offset += 192) {
        size_t part = raw_bytes - offset;
        if (part > 192) part = 192;
        size_t encoded_len = 0;
        if (mbedtls_base64_encode(encoded, sizeof(encoded), &encoded_len,
                                  raw + offset, part) == 0) {
            encoded[encoded_len] = '\0';
            printf("PC_TTS_B64:%s\n", encoded);
        }
    }
    printf("PC_TTS_TEXT_END\n");
    fflush(stdout);
}


static esp_err_t pc_tts_play(const char *text)
{
    if (!s_pc_tts_events) return ESP_ERR_INVALID_STATE;
    xEventGroupClearBits(s_pc_tts_events, PC_TTS_STARTED | PC_TTS_FINISHED | PC_TTS_ERROR);
    emit_pc_tts_text(text);
    EventBits_t bits = xEventGroupWaitBits(
        s_pc_tts_events, PC_TTS_STARTED | PC_TTS_ERROR, pdTRUE, pdFALSE,
        pdMS_TO_TICKS(35000));
    if (bits & PC_TTS_ERROR) return ESP_FAIL;
    if (!(bits & PC_TTS_STARTED)) {
        ESP_LOGE(TAG, "等待电脑 TTS 开始超时");
        return ESP_ERR_TIMEOUT;
    }
    bits = xEventGroupWaitBits(
        s_pc_tts_events, PC_TTS_FINISHED | PC_TTS_ERROR, pdTRUE, pdFALSE,
        pdMS_TO_TICKS(90000));
    if (bits & PC_TTS_ERROR) return ESP_FAIL;
    if (!(bits & PC_TTS_FINISHED)) {
        ESP_LOGE(TAG, "等待电脑 TTS 结束超时");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
#endif


static esp_http_client_handle_t new_client(const char *url)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = CLOUD_HTTP_TIMEOUT_MS,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    return esp_http_client_init(&cfg);
}


static bool append_text(char **answer, size_t *used, size_t *capacity, const char *part)
{
    size_t add = strlen(part);
    if (*used + add > 8192) add = 8192 - *used;
    if (!add) return true;
    if (*used + add + 1 > *capacity) {
        size_t next = *capacity ? *capacity * 2 : 512;
        while (next < *used + add + 1) next *= 2;
        char *grown = realloc(*answer, next);
        if (!grown) return false;
        *answer = grown; *capacity = next;
    }
    memcpy(*answer + *used, part, add);
    *used += add; (*answer)[*used] = 0;
    return true;
}

static void parse_sse_line(char *line, char **answer, size_t *used, size_t *capacity, bool *done)
{
    if (strncmp(line, "data:", 5) != 0) return;
    char *data = line + 5;
    while (*data == ' ') ++data;
    if (strcmp(data, "[DONE]") == 0) { *done = true; return; }
    cJSON *root = cJSON_Parse(data);
    cJSON *choices = root ? cJSON_GetObjectItemCaseSensitive(root, "choices") : NULL;
    cJSON *choice = cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
    cJSON *delta = choice ? cJSON_GetObjectItemCaseSensitive(choice, "delta") : NULL;
    cJSON *content = delta ? cJSON_GetObjectItemCaseSensitive(delta, "content") : NULL;
    if (cJSON_IsString(content)) append_text(answer, used, capacity, content->valuestring);
    cJSON_Delete(root);
}

static char *deepseek_stream(const char *prompt, bool *complete, bool *backend_failure)
{
    *complete = false;
    *backend_failure = false;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", DEEPSEEK_MODEL);
    cJSON_AddBoolToObject(root, "stream", true);
    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    cJSON *system = cJSON_CreateObject();
    cJSON_AddStringToObject(system, "role", "system");
    cJSON_AddStringToObject(system, "content", "你是赛博猫猫，回答简洁、自然，适合语音播报。");
    cJSON_AddItemToArray(messages, system);
    cJSON *user = cJSON_CreateObject();
    cJSON_AddStringToObject(user, "role", "user");
    cJSON_AddStringToObject(user, "content", prompt);
    cJSON_AddItemToArray(messages, user);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return NULL;

    esp_http_client_handle_t client = new_client("https://api.deepseek.com/chat/completions");
    if (!client) { free(body); return NULL; }
    char api_key[sizeof(s_deepseek_api_key)];
    if (!s_api_key_lock || xSemaphoreTake(s_api_key_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        free(body); esp_http_client_cleanup(client); return NULL;
    }
    strlcpy(api_key, s_deepseek_api_key, sizeof(api_key));
    xSemaphoreGive(s_api_key_lock);
    char auth[192]; snprintf(auth, sizeof(auth), "Bearer %s", api_key);
    memset(api_key, 0, sizeof(api_key));
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "text/event-stream");
    esp_err_t err = esp_http_client_open(client, strlen(body));
    if (err == ESP_OK) err = esp_http_client_write(client, body, strlen(body)) == (int)strlen(body) ? ESP_OK : ESP_FAIL;
    free(body);
    if (err != ESP_OK) {
        *backend_failure = true;
        esp_http_client_cleanup(client);
        return NULL;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        *backend_failure = true;
        ESP_LOGE(TAG, "DeepSeek 响应传输失败");
        esp_http_client_cleanup(client);
        return NULL;
    }
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        *backend_failure = true;
        ESP_LOGE(TAG, "DeepSeek 请求失败：HTTP=%d", status);
        esp_http_client_cleanup(client);
        return NULL;
    }

    char *answer = NULL, line[4096];
    size_t used = 0, capacity = 0, line_used = 0;
    bool done = false;
    uint8_t chunk[512];
    while (!done) {
        int n = esp_http_client_read(client, (char *)chunk, sizeof(chunk));
        if (n <= 0) break;
        for (int i = 0; i < n; ++i) {
            char ch = (char)chunk[i];
            if (ch == '\n') {
                if (line_used && line[line_used - 1] == '\r') --line_used;
                line[line_used] = 0;
                parse_sse_line(line, &answer, &used, &capacity, &done);
                line_used = 0;
            } else if (line_used < sizeof(line) - 1) {
                line[line_used++] = ch;
            }
        }
    }
    esp_http_client_cleanup(client);
    *complete = done;
    if (!done) *backend_failure = true;
    return answer;
}

static void handle_prompt(const char *prompt, const conversation_stamp_t *stamp)
{
    if (stamp) wake_led_stage(stamp->started_ms, WAKE_LED_STAGE_AI_REPLY);
    bool complete = false, backend_failure = false;
    int64_t deepseek_started_ms = monotonic_ms();
    char *answer = deepseek_stream(prompt, &complete, &backend_failure);
    log_latency(stamp, "DeepSeek", deepseek_started_ms);
    if (!answer || !*answer) {
        if (stamp) conversation_log_finish(*stamp,
            backend_failure ? CHATLOG_BACKEND_FAILED : CHATLOG_DEEPSEEK_FAILED, prompt, "");
        ESP_LOGE(TAG, "DeepSeek 未返回可用文字");
        free(answer);
        return;
    }
    /* Freeze this conversation immediately when the response stream ends.
     * No subsequent microphone/idle/TTS events are appended to this record. */
    if (stamp) conversation_log_finish(*stamp, complete ? CHATLOG_OK : CHATLOG_BACKEND_FAILED,
                                      prompt, answer);
    ESP_LOGI(TAG, "猫猫回复：%s", answer);
    if (stamp) wake_led_stage(stamp->started_ms, WAKE_LED_STAGE_TEXT_TO_SPEECH);
    int64_t tts_started_ms = monotonic_ms();
    esp_err_t tts_err;
#if PC_TTS_OVER_USB
    tts_err = pc_tts_play(answer);
#else
    tts_err = youdao_tts_stream_to_bridge(answer);
#endif
    log_latency(stamp, youdao_tts_speaker_mode() ? "TTS 到喇叭" : "TTS 到音频桥", tts_started_ms);
    if (stamp) wake_led_finished(stamp->started_ms,
        tts_err == ESP_OK ? WAKE_LED_SUCCESS : WAKE_LED_BACKEND_FAILED);
    free(answer);
}

static void cloud_task(void *arg)
{
    (void)arg;
    for (;;) {
        prompt_item_t queued;
        if (xQueueReceive(s_prompts, &queued, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (wifi_service_wait_connected(1000)) {
                if (queued.tts_only) {
                    int64_t started_ms = monotonic_ms();
                    wake_led_listening(started_ms);
                    wake_led_stage(started_ms, WAKE_LED_STAGE_TEXT_TO_SPEECH);
                    esp_err_t err = youdao_tts_stream_to_bridge(queued.text);
                    ESP_LOGI(TAG, "USB TTS 测试结果：%s", err == ESP_OK ? "成功" : "失败");
                    wake_led_finished(started_ms, err == ESP_OK ? WAKE_LED_SUCCESS : WAKE_LED_BACKEND_FAILED);
                } else {
                    handle_prompt(queued.text, NULL);
                }
            }
            continue;
        }
        speech_utterance_t utterance;
        if (sr_engine_take_utterance(&utterance, 100)) {
            ESP_LOGI(TAG, "延时 录音结束：音频=%u ms，唤醒后=%lld ms",
                     (unsigned)(utterance.sample_count / 16),
                     (long long)(monotonic_ms() - utterance.log_stamp.started_ms));
            if (wifi_service_wait_connected(3000)) {
                wake_led_stage(utterance.log_stamp.started_ms, WAKE_LED_STAGE_SPEECH_TO_TEXT);
                youdao_asr_status_t asr_status = YOUDAO_ASR_BACKEND_ERROR;
                int64_t asr_started_ms = monotonic_ms();
                char *text = youdao_asr_transcribe(&utterance, &asr_status);
                log_latency(&utterance.log_stamp, "有道识别", asr_started_ms);
                if (text) {
                    ESP_LOGI(TAG, "语音识别结果：%s", text);
                    handle_prompt(text, &utterance.log_stamp);
                    free(text);
                } else {
                    conversation_log_finish(utterance.log_stamp,
                        asr_status == YOUDAO_ASR_NO_RECOGNITION ?
                        CHATLOG_NO_RECOGNITION : CHATLOG_BACKEND_FAILED, "", "");
                }
            } else {
                conversation_log_finish(utterance.log_stamp, CHATLOG_OFFLINE, "", "");
                ESP_LOGW(TAG, "当前离线，开放式语音无法使用云端识别");
            }
            sr_engine_release_utterance(&utterance);
        }
    }
}

esp_err_t cloud_service_init(void)
{
    s_prompts = xQueueCreate(4, sizeof(prompt_item_t));
    s_pc_tts_events = xEventGroupCreate();
    s_api_key_lock = xSemaphoreCreateMutex();
    if (!s_prompts || !s_pc_tts_events || !s_api_key_lock) return ESP_ERR_NO_MEM;
    strlcpy(s_deepseek_api_key, DEEPSEEK_API_KEY, sizeof(s_deepseek_api_key));
    nvs_handle_t handle;
    if (nvs_open("cybercat", NVS_READONLY, &handle) == ESP_OK) {
        size_t length = sizeof(s_deepseek_api_key);
        if (nvs_get_str(handle, "deepseek", s_deepseek_api_key, &length) != ESP_OK) {
            strlcpy(s_deepseek_api_key, DEEPSEEK_API_KEY, sizeof(s_deepseek_api_key));
        }
        nvs_close(handle);
    }
    return xTaskCreate(cloud_task, "deepseek_tts_task", 12288, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t cloud_service_submit_prompt(const char *prompt)
{
    if (!s_prompts || !prompt) return ESP_ERR_INVALID_STATE;
    prompt_item_t item = {0};
    if (strlcpy(item.text, prompt, sizeof(item.text)) >= sizeof(item.text)) return ESP_ERR_INVALID_SIZE;
    return xQueueSend(s_prompts, &item, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t cloud_service_submit_tts_test(const char *text)
{
    if (!s_prompts || !text || !*text) return ESP_ERR_INVALID_STATE;
    prompt_item_t item = {.tts_only = true};
    if (strlcpy(item.text, text, sizeof(item.text)) >= sizeof(item.text)) return ESP_ERR_INVALID_SIZE;
    return xQueueSend(s_prompts, &item, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
