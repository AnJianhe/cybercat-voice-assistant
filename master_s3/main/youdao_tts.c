#include "youdao_tts.h"

#include "app_config.h"
#include "uart_link.h"
#include "youdao_config.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/i2s_std.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define YOUDAO_TTS_URL "https://openapi.youdao.com/ttsapi"
#define YOUDAO_TTS_VOICE "youxiaoqin"
#define TTS_HTTP_CHUNK 2048
/* WAV 的 fmt/data 头通常只有 44 字节；喇叭流式路径保留额外元数据余量。 */
#define TTS_WAV_PREFIX_CAPACITY 4096u

static const char *TAG = "有道合成";
static bool s_speaker;
static SemaphoreHandle_t s_output_lock;
static i2s_chan_handle_t s_tx;
static bool s_has_low_byte;
static uint8_t s_low_byte;
static uint32_t s_rate;
static volatile bool s_cancelled;

void youdao_tts_cancel(void) { s_cancelled = true; }

esp_err_t youdao_tts_output_init(void)
{
    s_output_lock = xSemaphoreCreateMutex();
    if (!s_output_lock) return ESP_ERR_NO_MEM;
    nvs_handle_t handle;
    uint8_t mode = 0;
    if (nvs_open("cybercat", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, "speaker", &mode);
        nvs_close(handle);
    }
    s_speaker = mode == 1;
    ESP_LOGI(TAG, "输出模式：%s", s_speaker ? "S3 直连 I2S 喇叭" : "WROOM 蓝牙音箱");
    return ESP_OK;
}

bool youdao_tts_speaker_mode(void) { return s_speaker; }

esp_err_t youdao_tts_set_speaker_mode(bool speaker)
{
    if (!s_output_lock || xSemaphoreTake(s_output_lock, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("cybercat", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "speaker", speaker ? 1 : 0);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err == ESP_OK) s_speaker = speaker;
    xSemaphoreGive(s_output_lock);
    return err;
}

static esp_err_t output_start(uint32_t rate, uint8_t bits, uint8_t channels)
{
    s_has_low_byte = false;
    if (!s_speaker) return uart_link_pcm_start(rate, bits, channels);
    if (bits != 16 || channels != 1) return ESP_ERR_NOT_SUPPORTED;
    s_rate = rate;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    channel.dma_desc_num = 4;
    channel.dma_frame_num = 128;
    channel.auto_clear = true;
    esp_err_t err = i2s_new_channel(&channel, &s_tx, NULL);
    if (err != ESP_OK) return err;
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = 15, .ws = 16, .dout = 7, .din = I2S_GPIO_UNUSED},
    };
    err = i2s_channel_init_std_mode(s_tx, &cfg);
    if (err == ESP_OK) err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) { i2s_del_channel(s_tx); s_tx = NULL; }
    else ESP_LOGI(TAG, "S3 喇叭输出就绪：BCLK=15，LRCLK=16，DIN=7，%u Hz", (unsigned)rate);
    return err;
}

static void output_abort(void)
{
    if (s_tx) { i2s_channel_disable(s_tx); i2s_del_channel(s_tx); s_tx = NULL; }
    else if (!s_speaker) uart_link_stop();
}

static esp_err_t output_end(void)
{
    if (s_has_low_byte) { output_abort(); return ESP_ERR_INVALID_SIZE; }
    if (!s_speaker) return uart_link_pcm_end();
    /* write 返回代表进入 DMA 队列；等待最大队列时长，保留句尾。 */
    vTaskDelay(pdMS_TO_TICKS((512000u + s_rate - 1) / s_rate + 20));
    output_abort();
    return ESP_OK;
}

static esp_err_t output_bytes(const uint8_t *data, size_t bytes)
{
    if (!s_speaker) return uart_link_pcm_data(data, bytes);
    size_t offset = 0;
    while (offset < bytes) {
        size_t written = 0;
        esp_err_t err = i2s_channel_write(s_tx, data + offset, bytes - offset, &written, 2000);
        if (err != ESP_OK || !written) return err == ESP_OK ? ESP_ERR_TIMEOUT : err;
        offset += written;
    }
    return ESP_OK;
}

static size_t utf8_count(const char *text)
{
    size_t count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if ((*p & 0xc0) != 0x80) ++count;
    return count;
}

static const char *utf8_offset(const char *text, size_t codepoints)
{
    size_t count = 0;
    for (const char *p = text; *p; ++p) {
        if (((unsigned char)*p & 0xc0) != 0x80 && count++ == codepoints) return p;
    }
    return text + strlen(text);
}

static char *signing_input(const char *text)
{
    size_t count = utf8_count(text);
    if (count <= 20) return strdup(text);
    const char *first_end = utf8_offset(text, 10);
    const char *last_start = utf8_offset(text, count - 10);
    size_t first = (size_t)(first_end - text), last = strlen(last_start);
    char *value = malloc(first + last + 24);
    if (value) snprintf(value, first + last + 24, "%.*s%u%s", (int)first, text,
                        (unsigned)count, last_start);
    return value;
}

static char *url_encode(const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t length = strlen(text);
    char *out = malloc(length * 3 + 1);
    if (!out) return NULL;
    size_t used = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out[used++] = (char)ch;
        } else {
            out[used++] = '%'; out[used++] = hex[ch >> 4]; out[used++] = hex[ch & 15];
        }
    }
    out[used] = 0;
    return out;
}

static bool make_signature(const char *text, char salt[37], char timestamp[24], char signature[65])
{
    time_t now = time(NULL);
    if (now < 1704067200) return false;
    uint8_t random[16], digest[32];
    esp_fill_random(random, sizeof(random));
    snprintf(salt, 37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             random[0], random[1], random[2], random[3], random[4], random[5], random[6], random[7],
             random[8], random[9], random[10], random[11], random[12], random[13], random[14], random[15]);
    snprintf(timestamp, 24, "%lld", (long long)now);
    char *input = signing_input(text);
    if (!input) return false;
    size_t n = strlen(YOUDAO_APP_ID) + strlen(input) + strlen(salt) + strlen(timestamp) + strlen(YOUDAO_APP_SECRET) + 1;
    char *signing = malloc(n);
    if (!signing) { free(input); return false; }
    snprintf(signing, n, "%s%s%s%s%s", YOUDAO_APP_ID, input, salt, timestamp, YOUDAO_APP_SECRET);
    free(input);
    int err = mbedtls_sha256((const unsigned char *)signing, strlen(signing), digest, 0);
    memset(signing, 0, n); free(signing);
    if (err) return false;
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(signature + i * 2, 3, "%02x", digest[i]);
    return true;
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* 定位 RIFF/WAVE 的 fmt 与 data 块。 */
static bool wav_locate(const uint8_t *buffer, size_t length, uint32_t *rate, uint8_t *bits,
                       uint8_t *channels, size_t *data_offset, size_t *data_length)
{
    if (length < 44 || memcmp(buffer, "RIFF", 4) != 0 || memcmp(buffer + 8, "WAVE", 4) != 0) return false;
    size_t pos = 12;
    bool have_fmt = false;
    while (pos + 8 <= length) {
        const uint8_t *id = buffer + pos;
        uint32_t size = read_le32(buffer + pos + 4);
        size_t payload = pos + 8;
        if (payload > length) return false;
        if (memcmp(id, "fmt ", 4) == 0 && size >= 16) {
            if ((size_t)size > length - payload) return false;
            uint16_t format = read_le16(buffer + payload);
            uint16_t channel_count = read_le16(buffer + payload + 2);
            uint32_t sample_rate = read_le32(buffer + payload + 4);
            uint16_t sample_bits = read_le16(buffer + payload + 14);
            if (format != 1 || channel_count < 1 || channel_count > 2 ||
                sample_bits != 16 || sample_rate < 8000 || sample_rate > 48000) return false;
            *rate = sample_rate;
            *channels = (uint8_t)channel_count;
            *bits = (uint8_t)sample_bits;
            have_fmt = true;
        } else if (memcmp(id, "data", 4) == 0 && have_fmt) {
            *data_offset = payload;
            /* The response is intentionally only partially buffered here. Keep
             * the RIFF-declared PCM size, not the bytes currently in prefix. */
            *data_length = size;
            return size > 0;
        }
        if ((size_t)size > length - payload) return false;
        pos = payload + size + (size & 1);
    }
    return false;
}

static esp_err_t send_pcm_chunks(const uint8_t *pcm, size_t length)
{
    uint8_t out[UART_PCM_CHUNK_BYTES];
    size_t used = 0;
    for (size_t i = 0; i < length; ++i) {
        if (s_cancelled) return ESP_ERR_INVALID_STATE;
        if (!s_has_low_byte) { s_low_byte = pcm[i]; s_has_low_byte = true; continue; }
        out[used++] = s_low_byte; out[used++] = pcm[i];
        if (s_speaker) { out[used++] = s_low_byte; out[used++] = pcm[i]; }
        s_has_low_byte = false;
        if (used == sizeof(out)) {
            if (output_bytes(out, used) != ESP_OK) return ESP_FAIL;
            used = 0;
        }
    }
    if (used && output_bytes(out, used) != ESP_OK) return ESP_FAIL;
    return ESP_OK;
}

/* Start the bridge as soon as the HTTP response contains a usable WAV header.
 * The old implementation downloaded the complete WAV to PSRAM before emitting a
 * single PCM byte, which added the whole download time to every reply. */
static esp_err_t stream_wav_response_to_bridge(esp_http_client_handle_t client)
{
    uint8_t prefix[TTS_WAV_PREFIX_CAPACITY];
    uint8_t chunk[TTS_HTTP_CHUNK];
    size_t prefix_used = 0, pcm_sent = 0, pcm_length = 0, data_offset = 0;
    uint32_t rate = 0;
    uint8_t bits = 0, channels = 0;
    bool started = false;
    int64_t request_ms = esp_timer_get_time() / 1000;

    for (;;) {
        int n = esp_http_client_read(client, (char *)chunk, sizeof(chunk));
        if (n < 0) {
            ESP_LOGE(TAG, "语音合成音频读取中断");
            if (started) output_abort();
            return ESP_FAIL;
        }
        if (n == 0) break;

        if (!started) {
            if (prefix_used + (size_t)n > sizeof(prefix)) {
                ESP_LOGE(TAG, "语音合成 WAV 头过长或格式无效");
                return ESP_FAIL;
            }
            memcpy(prefix + prefix_used, chunk, (size_t)n);
            prefix_used += (size_t)n;
            if (!wav_locate(prefix, prefix_used, &rate, &bits, &channels,
                            &data_offset, &pcm_length)) {
                continue;
            }
            if (output_start(rate, bits, channels) != ESP_OK) return ESP_FAIL;
            started = true;
            ESP_LOGI(TAG, "语音合成首段 PCM：%u ms（%u Hz / %u 位 / %u 声道）",
                     (unsigned)((esp_timer_get_time() / 1000) - request_ms),
                     (unsigned)rate, (unsigned)bits, (unsigned)channels);
            size_t available = prefix_used - data_offset;
            if (available > pcm_length) available = pcm_length;
            if (available && send_pcm_chunks(prefix + data_offset, available) != ESP_OK) {
                output_abort();
                return ESP_FAIL;
            }
            pcm_sent = available;
            continue;
        }

        size_t available = (size_t)n;
        if (available > pcm_length - pcm_sent) available = pcm_length - pcm_sent;
        if (available && send_pcm_chunks(chunk, available) != ESP_OK) {
            output_abort();
            return ESP_FAIL;
        }
        pcm_sent += available;
    }

    if (!started || !esp_http_client_is_complete_data_received(client) || pcm_sent != pcm_length) {
        ESP_LOGE(TAG, "语音合成 WAV 不完整：%u/%u PCM 字节", (unsigned)pcm_sent, (unsigned)pcm_length);
        if (started) output_abort();
        return ESP_FAIL;
    }
    esp_err_t result = output_end();
    ESP_LOGI(TAG, "语音合成 PCM 已送往%s：%u 字节，耗时 %u ms，结果=%s", s_speaker ? "S3 喇叭" : "音频桥", (unsigned)pcm_sent,
             (unsigned)((esp_timer_get_time() / 1000) - request_ms), result == ESP_OK ? "成功" : "失败");
    return result;
}

static esp_err_t cached_bluetooth_response(esp_http_client_handle_t client)
{
    int64_t content_length = esp_http_client_get_content_length(client);
    /* 有道可能使用 chunked 响应；未知长度按需扩容，不一次占满 PSRAM。 */
    const size_t limit = 3 * 1024 * 1024;
    if (content_length > (int64_t)limit) return stream_wav_response_to_bridge(client);
    size_t capacity = content_length > 0 ? (size_t)content_length : 65536;
    uint8_t *wav = heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!wav) return stream_wav_response_to_bridge(client);
    esp_err_t err = ESP_FAIL;
    size_t used = 0, offset = 0, bytes = 0;
    uint32_t rate = 0;
    uint8_t bits = 0, channels = 0;
    bool started = false;
    int64_t began = esp_timer_get_time();
    while (!esp_http_client_is_complete_data_received(client) && !s_cancelled) {
        if (used == capacity) {
            if (capacity == limit) goto done;
            size_t next = capacity * 2;
            if (next > limit) next = limit;
            uint8_t *expanded = heap_caps_realloc(wav, next, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!expanded) goto done;
            wav = expanded;
            capacity = next;
        }
        size_t wanted = capacity - used;
        if (wanted > 8192) wanted = 8192;
        int n = esp_http_client_read(client, (char *)wav + used, (int)wanted);
        if (n <= 0) goto done;
        used += (size_t)n;
    }
    if ((content_length > 0 && used != (size_t)content_length) || !esp_http_client_is_complete_data_received(client) ||
        !wav_locate(wav, used, &rate, &bits, &channels, &offset, &bytes) ||
        offset > used || bytes > used - offset || s_cancelled) goto done;
    esp_http_client_close(client);
    ESP_LOGI(TAG, "蓝牙音频已下载到 PSRAM：%u 字节，耗时 %u ms；开始播放",
             (unsigned)used, (unsigned)((esp_timer_get_time() - began) / 1000));
    err = output_start(rate, bits, channels);
    if (err != ESP_OK) goto done;
    started = true;
    err = send_pcm_chunks(wav + offset, bytes);
    if (err == ESP_OK) err = output_end();
    if (err == ESP_OK) started = false;
done:
    if (started) output_abort();
    free(wav);
    ESP_LOGI(TAG, "蓝牙 PCM 输出：%u 字节，结果=%s", (unsigned)bytes, err == ESP_OK ? "成功" : "失败");
    return err;
}

static esp_err_t request_tts(const char *text)
{
    if (!text || !*text || !YOUDAO_APP_ID[0] || !YOUDAO_APP_SECRET[0]) return ESP_ERR_INVALID_STATE;
    char salt[37], timestamp[24], signature[65];
    if (!make_signature(text, salt, timestamp, signature)) return ESP_FAIL;
    char *encoded = url_encode(text);
    if (!encoded) return ESP_ERR_NO_MEM;
    size_t body_size = strlen(YOUDAO_APP_ID) + strlen(salt) + strlen(timestamp) + strlen(signature) + strlen(encoded) + 160;
    char *body = malloc(body_size);
    if (!body) { free(encoded); return ESP_ERR_NO_MEM; }
    snprintf(body, body_size,
             "appKey=%s&salt=%s&curtime=%s&sign=%s&signType=v3&format=wav&speed=1&volume=1&voiceName=%s&q=%s",
             YOUDAO_APP_ID, salt, timestamp, signature, YOUDAO_TTS_VOICE, encoded);
    free(encoded);
    esp_http_client_config_t config = {.url = YOUDAO_TTS_URL, .timeout_ms = CLOUD_HTTP_TIMEOUT_MS,
        .buffer_size = 2048, .buffer_size_tx = 2048, .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { free(body); return ESP_ERR_NO_MEM; }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_err_t err = esp_http_client_open(client, strlen(body));
    if (err == ESP_OK) err = esp_http_client_write(client, body, strlen(body)) == (int)strlen(body) ? ESP_OK : ESP_FAIL;
    free(body);
    if (err != ESP_OK || esp_http_client_fetch_headers(client) < 0 || esp_http_client_get_status_code(client) != 200) {
        ESP_LOGE(TAG, "语音合成请求失败：HTTP=%d", esp_http_client_get_status_code(client));
        esp_http_client_cleanup(client); return ESP_FAIL;
    }
    esp_err_t result = s_speaker ? stream_wav_response_to_bridge(client) : cached_bluetooth_response(client);
    esp_http_client_cleanup(client);
    return result;
}

esp_err_t youdao_tts_stream_to_bridge(const char *text)
{
    if (!s_output_lock || xSemaphoreTake(s_output_lock, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    s_cancelled = false;
    esp_err_t err = request_tts(text);
    xSemaphoreGive(s_output_lock);
    return err;
}
