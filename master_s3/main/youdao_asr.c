#include "youdao_asr.h"
#include "youdao_config.h"
#include "app_config.h"

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "有道识别";

static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static void *audio_alloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static bool write_all(esp_http_client_handle_t client, const char *data, size_t length,
                      size_t *sent_bytes)
{
    while (length) {
        int n = esp_http_client_write(client, data, length > 8192 ? 8192 : (int)length);
        if (n <= 0) return false;
        *sent_bytes += (size_t)n;
        data += n;
        length -= n;
    }
    return true;
}

static bool needs_escape(char ch) { return ch == '+' || ch == '/' || ch == '='; }

static bool write_encoded_audio(esp_http_client_handle_t client, const char *q,
                                size_t *sent_bytes)
{
    static const char hex[] = "0123456789ABCDEF";
    /* PSRAM 中批量编码；减少小 TLS 写入，不增加 cloud_task 的栈占用。 */
    const size_t capacity = 8192;
    char *buffer = audio_alloc(capacity);
    if (!buffer) return false;
    size_t used = 0;
    for (; *q; ++q) {
        if (used + 3 > capacity) {
            if (!write_all(client, buffer, used, sent_bytes)) { free(buffer); return false; }
            used = 0;
        }
        unsigned char ch = (unsigned char)*q;
        if (needs_escape(ch)) {
            buffer[used++] = '%';
            buffer[used++] = hex[ch >> 4];
            buffer[used++] = hex[ch & 15];
        } else {
            buffer[used++] = ch;
        }
    }
    bool result = write_all(client, buffer, used, sent_bytes);
    free(buffer);
    return result;
}

char *youdao_asr_transcribe(const speech_utterance_t *utterance, youdao_asr_status_t *status)
{
    const int64_t started_us = esp_timer_get_time();
    if (status) *status = YOUDAO_ASR_BACKEND_ERROR;
    if (!utterance || !utterance->samples || !utterance->sample_count ||
        utterance->sample_count > 16000U * 60U) {
        if (status) *status = YOUDAO_ASR_NO_RECOGNITION;
        return NULL;
    }
    if (!YOUDAO_APP_ID[0] || !YOUDAO_APP_SECRET[0]) {
        ESP_LOGE(TAG, "有道服务尚未配置密钥");
        return NULL;
    }
    /* appKey is a hexadecimal application ID, not a Bearer API key. */
    for (const char *p = YOUDAO_APP_ID; *p; ++p) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
              (*p >= 'A' && *p <= 'F'))) {
            ESP_LOGE(TAG, "有道应用 ID 格式无效");
            return NULL;
        }
    }
    time_t now = time(NULL);
    for (int retry = 0; now < 1704067200 && retry < 150; ++retry) {
        vTaskDelay(pdMS_TO_TICKS(100));
        now = time(NULL);
    }
    if (now < 1704067200) {
        ESP_LOGE(TAG, "时钟尚未同步，无法签名语音识别请求");
        return NULL;
    }
    size_t pcm_bytes = utterance->sample_count * sizeof(int16_t);
    size_t wav_size = pcm_bytes + 44;
    uint8_t *wav = audio_alloc(wav_size);
    if (!wav) { ESP_LOGE(TAG, "可用内存不足，无法创建识别音频"); return NULL; }
    memcpy(wav, "RIFF", 4); put32(wav + 4, pcm_bytes + 36);
    memcpy(wav + 8, "WAVEfmt ", 8); put32(wav + 16, 16);
    put16(wav + 20, 1); put16(wav + 22, 1);
    put32(wav + 24, 16000); put32(wav + 28, 32000);
    put16(wav + 32, 2); put16(wav + 34, 16);
    memcpy(wav + 36, "data", 4); put32(wav + 40, pcm_bytes);
    memcpy(wav + 44, utterance->samples, pcm_bytes);
    size_t q_capacity = 4 * ((wav_size + 2) / 3) + 1, q_length = 0;
    char *q = audio_alloc(q_capacity);
    int encoded = q ? mbedtls_base64_encode((unsigned char *)q, q_capacity,
                                            &q_length, wav, wav_size) : -1;
    free(wav);
    if (encoded != 0) {
        free(q);
        ESP_LOGE(TAG, "识别音频编码失败");
        return NULL;
    }
    q[q_length] = 0;
    char input[48];
    if (q_length > 20) snprintf(input, sizeof(input), "%.10s%u%.10s",
                               q, (unsigned)q_length, q + q_length - 10);
    else snprintf(input, sizeof(input), "%s", q);
    uint8_t random[16];
    esp_fill_random(random, sizeof(random));
    random[6] = (random[6] & 0x0f) | 0x40;
    random[8] = (random[8] & 0x3f) | 0x80;
    char salt[37], timestamp[24], signature[65];
    snprintf(salt, sizeof(salt),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             random[0], random[1], random[2], random[3], random[4], random[5],
             random[6], random[7], random[8], random[9], random[10], random[11],
             random[12], random[13], random[14], random[15]);
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)now);
    size_t signing_size = strlen(YOUDAO_APP_ID) + strlen(input) + strlen(salt) +
                          strlen(timestamp) + strlen(YOUDAO_APP_SECRET) + 1;
    char *signing = malloc(signing_size);
    if (!signing) { free(q); return NULL; }
    snprintf(signing, signing_size, "%s%s%s%s%s", YOUDAO_APP_ID, input, salt,
             timestamp, YOUDAO_APP_SECRET);
    uint8_t digest[32];
    int hash_error = mbedtls_sha256((const unsigned char *)signing, strlen(signing), digest, 0);
    memset(signing, 0, signing_size);
    free(signing);
    if (hash_error) { free(q); return NULL; }
    for (size_t i = 0; i < sizeof(digest); ++i)
        snprintf(signature + i * 2, 3, "%02x", digest[i]);

    char prefix[512];
    int prefix_length = snprintf(prefix, sizeof(prefix),
        "appKey=%s&salt=%s&curtime=%s&sign=%s&signType=%s&langType=%s"
        "&format=wav&rate=16000&channel=1&type=1&q=",
        YOUDAO_APP_ID, salt, timestamp, signature, YOUDAO_ASR_SIGN_TYPE, YOUDAO_ASR_LANGUAGE);
    if (prefix_length < 0 || prefix_length >= (int)sizeof(prefix)) { free(q); return NULL; }
    size_t encoded_length = q_length;
    for (size_t i = 0; i < q_length; ++i) if (needs_escape(q[i])) encoded_length += 2;
    esp_http_client_config_t config = {
        .url = YOUDAO_ASR_URL, .timeout_ms = CLOUD_HTTP_TIMEOUT_MS,
        .buffer_size = 2048, .buffer_size_tx = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { free(q); return NULL; }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_header(client, "Accept", "application/json");
    const int64_t prepared_us = esp_timer_get_time();
    ESP_LOGI(TAG, "正在提交 %u ms 语音", (unsigned)(utterance->sample_count / 16));
    /* Collect timings without printing between network operations: USB log
     * back-pressure would otherwise become part of the measured request. */
    int64_t phase_us[6] = {prepared_us - started_us, -1, -1, -1, -1, -1};
    size_t sent_bytes = 0;
    char *result = NULL;
    char response[4096] = {0};
    size_t used = 0;
    int64_t phase_started_us = esp_timer_get_time();
    esp_err_t open_result = esp_http_client_open(client, prefix_length + encoded_length);
    phase_us[1] = esp_timer_get_time() - phase_started_us;
    if (open_result != ESP_OK) {
        free(q);
        ESP_LOGE(TAG, "语音识别网络建连失败：%s", esp_err_to_name(open_result));
        goto cleanup;
    }
    phase_started_us = esp_timer_get_time();
    bool sent = write_all(client, prefix, prefix_length, &sent_bytes) &&
                write_encoded_audio(client, q, &sent_bytes);
    phase_us[2] = esp_timer_get_time() - phase_started_us;
    free(q);
    if (!sent) {
        ESP_LOGE(TAG, "语音识别音频上传失败");
        goto cleanup;
    }
    phase_started_us = esp_timer_get_time();
    int64_t response_length = esp_http_client_fetch_headers(client);
    phase_us[3] = esp_timer_get_time() - phase_started_us;
    if (response_length < 0) {
        ESP_LOGE(TAG, "等待语音识别响应失败");
        goto cleanup;
    }
    int http_status = esp_http_client_get_status_code(client);
    if (http_status != 200) { ESP_LOGE(TAG, "语音识别请求失败：HTTP=%d", http_status); goto cleanup; }
    phase_started_us = esp_timer_get_time();
    while (used < sizeof(response) - 1) {
        int n = esp_http_client_read(client, response + used, sizeof(response) - 1 - used);
        if (n < 0) {
            phase_us[4] = esp_timer_get_time() - phase_started_us;
            ESP_LOGE(TAG, "读取语音识别响应失败"); goto cleanup;
        }
        if (n == 0) break;
        used += n;
    }
    phase_us[4] = esp_timer_get_time() - phase_started_us;
    if (!esp_http_client_is_complete_data_received(client)) {
        ESP_LOGE(TAG, "语音识别响应不完整或过大");
        goto cleanup;
    }
    phase_started_us = esp_timer_get_time();
    cJSON *root = cJSON_Parse(response);
    cJSON *error = root ? cJSON_GetObjectItemCaseSensitive(root, "errorCode") : NULL;
    if (cJSON_IsString(error) && strcmp(error->valuestring, "0") == 0) {
        cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "result");
        cJSON *first = cJSON_IsArray(items) ? cJSON_GetArrayItem(items, 0) : NULL;
        if (cJSON_IsString(first) && first->valuestring[0]) {
            result = strdup(first->valuestring);
            if (result && status) *status = YOUDAO_ASR_OK;
        } else if (status) {
            *status = YOUDAO_ASR_NO_RECOGNITION;
        }
    } else if (cJSON_IsString(error) && strcmp(error->valuestring, "4304") == 0) {
        /* Youdao's empty-recognition result: transport/backend are healthy. */
        if (status) *status = YOUDAO_ASR_NO_RECOGNITION;
        ESP_LOGI(TAG, "语音识别完成，但没有识别到文字");
    } else {
        ESP_LOGW(TAG, "语音识别服务返回错误：%s", cJSON_IsString(error) ? error->valuestring : "响应格式无效");
    }
    cJSON_Delete(root);
    phase_us[5] = esp_timer_get_time() - phase_started_us;
cleanup:
    {
        static const char *const phases[] = {
            "本地准备", "网络建连", "音频上传", "等待响应", "读取结果", "解析结果"
        };
        const int64_t measured_ms = (esp_timer_get_time() - started_us) / 1000;
        for (size_t i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
            if (phase_us[i] >= 0)
                ESP_LOGI(TAG, "识别分段 %s=%lld ms", phases[i], (long long)(phase_us[i] / 1000));
        }
        ESP_LOGI(TAG, "识别请求计时：上传=%u/%u 字节，响应=%u 字节，HTTP=%d，总计=%lld ms",
                 (unsigned)sent_bytes, (unsigned)(prefix_length + encoded_length),
                 (unsigned)used, esp_http_client_get_status_code(client), (long long)measured_ms);
    }
    esp_http_client_cleanup(client);
    return result;
}
