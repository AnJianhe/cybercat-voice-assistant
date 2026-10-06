#include "sr_engine.h"
#include "app_config.h"
#include "audio_capture.h"
#include "cloud_service.h"
#include "youdao_tts.h"
#include "espnow_control.h"
#include "uart_link.h"
#include "wake_led.h"
#include "wifi_service.h"

#include "esp_afe_sr_models.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mbedtls/base64.h"
#if MIC_EXPORT_DIAGNOSTIC
#include "driver/usb_serial_jtag.h"
#endif
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_vad.h"
#include "model_path.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    CMD_AC_ON = 1,
    CMD_AC_OFF,
    CMD_TV_ON,
    CMD_TV_OFF,
    CMD_STOP_PLAYBACK,
    CMD_PLAY_MUSIC,
};

static const char *TAG = "语音引擎";
static const esp_afe_sr_iface_t *s_afe;
static esp_afe_sr_data_t *s_afe_data;
static srmodel_list_t *s_models;
static QueueHandle_t s_utterances;
static volatile bool s_feed_ready;

static void execute_fixed_command(int command_id)
{
    switch (command_id) {
    case CMD_AC_ON:  espnow_control_send("AC_ON"); break;
    case CMD_AC_OFF: espnow_control_send("AC_OFF"); break;
    case CMD_TV_ON:  espnow_control_send("TV_ON"); break;
    case CMD_TV_OFF: espnow_control_send("TV_OFF"); break;
    case CMD_STOP_PLAYBACK: uart_link_stop(); break;
    case CMD_PLAY_MUSIC: uart_link_command("PLAY:/music/song001.mp3"); break;
    default: break;
    }
}

#if MIC_EXPORT_DIAGNOSTIC
static int hex_nibble(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool decode_hex_text(const char *hex, char *out, size_t out_size)
{
    size_t length = strlen(hex);
    if ((length & 1) || length / 2 >= out_size) return false;
    for (size_t i = 0; i < length; i += 2) {
        int hi = hex_nibble(hex[i]), lo = hex_nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i / 2] = (char)((hi << 4) | lo);
    }
    out[length / 2] = 0;
    return true;
}

static bool mic_capture_requested(void)
{
    static char command[256];
    static size_t used;
    static bool overflow;
    char incoming[32];
    bool requested = false;
    int count = usb_serial_jtag_read_bytes(incoming, sizeof(incoming), 0);
    for (int i = 0; i < count; ++i) {
        if (incoming[i] == '\n') {
            command[used] = '\0';
            if (!overflow && strcmp(command, "MIC_RECORD") == 0) requested = true;
            else if (!overflow && strcmp(command, "S3_STATUS") == 0) {
                printf("@S3 STATUS {\"wifiConnected\":%s,\"ssid\":\"%s\",\"freeHeap\":%u,\"outputMode\":\"%s\"}\n",
                       wifi_service_wait_connected(0) ? "true" : "false", wifi_service_configured_ssid(),
                       (unsigned)esp_get_free_heap_size(), youdao_tts_speaker_mode() ? "speaker" : "bluetooth");
                fflush(stdout);
            }
            else if (!overflow && strncmp(command, "S3_OUTPUT_SET ", 14) == 0) {
                const char *mode = command + 14;
                esp_err_t err = (strcmp(mode, "speaker") == 0 || strcmp(mode, "bluetooth") == 0) ?
                    youdao_tts_set_speaker_mode(strcmp(mode, "speaker") == 0) : ESP_ERR_INVALID_ARG;
                printf("@S3 OUTPUT_SET %s\n", err == ESP_OK ? "OK" : esp_err_to_name(err));
                fflush(stdout);
            }
            else if (!overflow && strcmp(command, "S3_WIFI_SCAN") == 0) {
                wifi_service_scan_usb();
            }
            else if (!overflow && strncmp(command, "S3_WIFI_SET ", 12) == 0) {
                char *ssid_hex = command + 12;
                char *space = strchr(ssid_hex, ' ');
                char ssid[33], password[65];
                esp_err_t err = ESP_ERR_INVALID_ARG;
                if (space) {
                    *space++ = 0;
                    if (decode_hex_text(ssid_hex, ssid, sizeof(ssid)) &&
                        decode_hex_text(space, password, sizeof(password))) {
                        err = wifi_service_set_credentials(ssid, password);
                    }
                }
                memset(password, 0, sizeof(password));
                printf("@S3 WIFI_SET %s\n", err == ESP_OK ? "OK" : esp_err_to_name(err));
                fflush(stdout);
            }
            else if (!overflow && strncmp(command, "S3_TTS_TEST ", 12) == 0) {
                char text[512];
                esp_err_t err = decode_hex_text(command + 12, text, sizeof(text)) ?
                    cloud_service_submit_tts_test(text) : ESP_ERR_INVALID_ARG;
                printf("@S3 TTS_TEST %s\n", err == ESP_OK ? "QUEUED" : esp_err_to_name(err));
                fflush(stdout);
                memset(text, 0, sizeof(text));
            }
            else if (!overflow && strcmp(command, "S3_RESTART") == 0) {
                printf("@S3 RESTARTING\n");
                fflush(stdout);
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_restart();
            }
            else if (!overflow && strncmp(command, "S3_LINK_TEST", 12) == 0) {
                int chunks = atoi(command + 12);
                if (chunks <= 0) chunks = 100;
                printf("@S3 LINK_TEST %d\n", chunks);
                fflush(stdout);
                uart_link_selftest(chunks);
            }
            else if (!overflow) conversation_log_command(command);
            used = 0;
            overflow = false;
        } else if (incoming[i] != '\r') {
            if (used < sizeof(command) - 1) command[used++] = incoming[i];
            else overflow = true;
        }
    }
    return requested;
}
#endif

static void audio_capture_task(void *arg)
{
    (void)arg;
    int chunks = s_afe->get_feed_chunksize(s_afe_data);
    int channels = s_afe->get_feed_channel_num(s_afe_data);
    if (channels != 1) {
        ESP_LOGE(TAG, "语音前端需要 %d 路输入，但当前驱动只提供一路麦克风", channels);
        vTaskDelete(NULL);
    }
    int16_t *buffer = heap_caps_malloc((size_t)chunks * sizeof(int16_t), MALLOC_CAP_INTERNAL);
    if (!buffer) vTaskDelete(NULL);
    while (!s_feed_ready) vTaskDelay(pdMS_TO_TICKS(10));
#if MIC_EXPORT_DIAGNOSTIC
    const size_t capture_samples = 16000U * 24U;
    int16_t *capture = NULL;
    size_t capture_used = 0;
    int64_t capture_start_us = 0;
    bool capture_usb_ready = usb_serial_jtag_is_driver_installed();
    if (!capture_usb_ready) {
        usb_serial_jtag_driver_config_t usb_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        capture_usb_ready = usb_serial_jtag_driver_install(&usb_config) == ESP_OK;
    }
    if (capture_usb_ready) ESP_LOGI(TAG, "麦克风导出功能就绪：USB 命令 MIC_RECORD，时长 24 秒");
    else ESP_LOGE(TAG, "麦克风导出功能不可用：USB 驱动异常");
#endif
#if MIC_DIAGNOSTIC_LOG
    int32_t report_peak = 0;
    size_t report_samples = 0;
    size_t report_zeros[2] = {0, 0};
    int64_t report_start_us = esp_timer_get_time();
    int64_t next_report_us = report_start_us + 1000000;
#endif
    for (;;) {
        size_t got = 0;
        if (audio_capture_read(buffer, chunks, &got, 1000) == ESP_OK && got == (size_t)chunks) {
#if MIC_EXPORT_DIAGNOSTIC
            int64_t capture_now_us = esp_timer_get_time();
            if (capture_usb_ready && mic_capture_requested()) {
                if (capture) {
                    ESP_LOGW(TAG, "麦克风导出任务正在进行");
                } else {
                    capture = heap_caps_malloc(capture_samples * sizeof(int16_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    if (capture) {
                        capture_used = 0;
                        capture_start_us = capture_now_us + 3000000;
                        ESP_LOGI(TAG, "麦克风导出已准备：3 秒后开始，录制 24 秒");
                    } else {
                        ESP_LOGE(TAG, "麦克风导出失败：内存不足");
                    }
                }
            }
            if (capture && capture_now_us >= capture_start_us) {
                if (capture_used == 0) ESP_LOGI(TAG, "麦克风导出正在录音");
                size_t take = got;
                if (take > capture_samples - capture_used) take = capture_samples - capture_used;
                memcpy(capture + capture_used, buffer, take * sizeof(int16_t));
                capture_used += take;
                if (capture_used == capture_samples) {
                    const unsigned char *raw = (const unsigned char *)capture;
                    const size_t raw_bytes = capture_samples * sizeof(int16_t);
                    unsigned char encoded[257]; /* 192 raw bytes -> 256 base64 bytes + NUL */
                    printf("MIC_CAPTURE_DATA_BEGIN:%u\n", (unsigned)raw_bytes);
                    for (size_t offset = 0; offset < raw_bytes; offset += 192) {
                        size_t part = raw_bytes - offset;
                        if (part > 192) part = 192;
                        size_t encoded_len = 0;
                        if (mbedtls_base64_encode(encoded, sizeof(encoded), &encoded_len,
                                                  raw + offset, part) == 0) {
                            encoded[encoded_len] = '\0';
                            printf("MICB64:%s\n", encoded);
                            fflush(stdout);
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                    }
                    printf("MIC_CAPTURE_DATA_END\n");
                    fflush(stdout);
                    free(capture);
                    capture = NULL;
                    capture_used = 0;
                    ESP_LOGI(TAG, "麦克风导出完成，可再次使用 USB 命令 MIC_RECORD");
                }
            }
#endif
#if MIC_DIAGNOSTIC_LOG
            for (size_t i = 0; i < got; ++i) {
                int32_t magnitude = buffer[i];
                if (magnitude < 0) magnitude = -magnitude;
                if (magnitude > report_peak) report_peak = magnitude;
                if (buffer[i] == 0) report_zeros[(report_samples + i) & 1]++;
            }
            report_samples += got;
            int64_t now_us = esp_timer_get_time();
            if (now_us >= next_report_us) {
                ESP_LOGI(TAG, "麦克风峰值=%ld，采样率=%lu，偶数采样零值=%u%%，奇数采样零值=%u%%",
                         (long)report_peak,
                         (unsigned long)((uint64_t)report_samples * 1000000U / (now_us - report_start_us)),
                         (unsigned)(report_zeros[0] * 100U / ((report_samples + 1U) / 2U)),
                         (unsigned)(report_zeros[1] * 100U / (report_samples / 2U)));
                report_peak = 0;
                report_samples = 0;
                report_zeros[0] = report_zeros[1] = 0;
                report_start_us = now_us;
                next_report_us = now_us + 1000000;
            }
#endif
            s_afe->feed(s_afe_data, buffer);
        }
    }
}

static void esp_sr_task(void *arg)
{
    (void)arg;
    char *mn_name = esp_srmodel_filter(s_models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    if (!mn_name) {
        ESP_LOGE(TAG, "模型分区没有中文 MultiNet 模型；请检查配置和完整烧录");
        vTaskDelete(NULL);
    }
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *mn_data = multinet ? multinet->create(mn_name, 6000) : NULL;
    if (!mn_data) {
        ESP_LOGE(TAG, "创建 MultiNet 模型失败：%s", mn_name);
        vTaskDelete(NULL);
    }
    ESP_ERROR_CHECK(esp_mn_commands_alloc(multinet, mn_data));
    ESP_ERROR_CHECK(esp_mn_commands_add(CMD_AC_ON, "da kai kong tiao"));
    ESP_ERROR_CHECK(esp_mn_commands_add(CMD_AC_OFF, "guan bi kong tiao"));
    ESP_ERROR_CHECK(esp_mn_commands_add(CMD_TV_ON, "da kai dian shi"));
    ESP_ERROR_CHECK(esp_mn_commands_add(CMD_TV_OFF, "guan bi dian shi"));
    ESP_ERROR_CHECK(esp_mn_commands_add(CMD_STOP_PLAYBACK, "ting zhi bo fang"));
    ESP_ERROR_CHECK(esp_mn_commands_add(CMD_PLAY_MUSIC, "bo fang yin yue"));
    esp_mn_error_t *command_error = esp_mn_commands_update();
    if (command_error) ESP_LOGW(TAG, "部分离线指令未被模型接受；请查看启动日志");
    multinet->print_active_speech_commands(mn_data);

    int afe_chunk = s_afe->get_fetch_chunksize(s_afe_data);
    int mn_chunk = multinet->get_samp_chunksize(mn_data);
    if (afe_chunk != mn_chunk) {
        ESP_LOGE(TAG, "语音前端块大小 %d 与 MultiNet 块大小 %d 不一致", afe_chunk, mn_chunk);
        vTaskDelete(NULL);
    }

    const size_t max_samples = 16000U * OPEN_UTTERANCE_MAX_SECONDS;
    int16_t *recording = heap_caps_malloc(max_samples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!recording) recording = heap_caps_malloc(max_samples * sizeof(int16_t), MALLOC_CAP_8BIT);
    if (!recording) {
        ESP_LOGE(TAG, "无法分配语句录音缓冲区");
        vTaskDelete(NULL);
    }

    bool listening = false, fixed_handled = false, heard_speech = false;
    conversation_stamp_t log_stamp = {0};
    size_t recorded = 0, silent_samples = 0;
    s_feed_ready = true;
    wake_led_ready();
    for (;;) {
        afe_fetch_result_t *res = s_afe->fetch(s_afe_data);
        if (!res || res->ret_value == ESP_FAIL) continue;

        if (res->wakeup_state == WAKENET_DETECTED || res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            uart_link_stop(); /* Barge-in: immediately stop Bridge; WakeNet keeps running during playback. */
            multinet->clean(mn_data);
            listening = true;
            fixed_handled = false;
            heard_speech = false;
            recorded = silent_samples = 0;
            log_stamp = conversation_log_begin();
            wake_led_listening(log_stamp.started_ms);
            ESP_LOGI(TAG, "已唤醒，正在聆听");
        }
        if (!listening) continue;

        size_t copy = (size_t)afe_chunk;
        if (recorded + copy > max_samples) copy = max_samples - recorded;
        if (copy) {
            memcpy(recording + recorded, res->data, copy * sizeof(int16_t));
            recorded += copy;
        }
        if (res->vad_state == VAD_SPEECH) {
            heard_speech = true;
            silent_samples = 0;
        } else if (heard_speech) {
            silent_samples += afe_chunk;
        }

        esp_mn_state_t state = multinet->detect(mn_data, res->data);
        if (state == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *result = multinet->get_results(mn_data);
            if (result && result->num > 0) {
                ESP_LOGI(TAG, "识别到离线指令：编号=%d，置信度=%.3f", result->command_id[0], result->prob[0]);
                execute_fixed_command(result->command_id[0]);
                if (!fixed_handled) {
                    char description[48];
                    snprintf(description, sizeof(description), "fixed command id=%d", result->command_id[0]);
                    conversation_log_finish(log_stamp, CHATLOG_FIXED_COMMAND, description, "");
                }
                fixed_handled = true;
            }
        }

        bool silence_done = heard_speech && silent_samples >= (16000U * OPEN_UTTERANCE_END_SILENCE_MS / 1000U);
        bool full = recorded >= max_samples;
        bool no_speech_timeout = !heard_speech &&
            recorded >= (16000U * OPEN_UTTERANCE_START_TIMEOUT_MS / 1000U);
        /* MultiNet's command timeout must not truncate an active cloud utterance. */
        if (silence_done || full || no_speech_timeout) {
            if (!fixed_handled && heard_speech && recorded > 1600) {
                speech_utterance_t item = {
                    .samples = heap_caps_malloc(recorded * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                    .sample_count = recorded,
                    .log_stamp = log_stamp,
                };
                if (!item.samples) item.samples = malloc(recorded * sizeof(int16_t));
                if (item.samples) {
                    memcpy(item.samples, recording, recorded * sizeof(int16_t));
                    if (xQueueSend(s_utterances, &item, 0) != pdTRUE) {
                        free(item.samples);
                        conversation_log_finish(log_stamp, CHATLOG_QUEUE_FULL, "", "");
                    }
                } else {
                    conversation_log_finish(log_stamp, CHATLOG_NO_MEMORY, "", "");
                }
            } else if (!fixed_handled) {
                conversation_log_finish(log_stamp, CHATLOG_NO_SPEECH, "", "");
            }
            listening = false;
            s_afe->enable_wakenet(s_afe_data);
        }
    }
}

esp_err_t sr_engine_init(void)
{
    s_utterances = xQueueCreate(2, sizeof(speech_utterance_t));
    if (!s_utterances) return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(audio_capture_init(), TAG, "microphone");
    s_models = esp_srmodel_init("model");
    if (!s_models) return ESP_ERR_NOT_FOUND;
    afe_config_t *config = afe_config_init("M", s_models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (!config) return ESP_ERR_NO_MEM;
    config->aec_init = false;
    config->se_init = false;
    config->vad_init = true;
    config->wakenet_init = true;
    ESP_LOGI(TAG, "唤醒词模型=%s", config->wakenet_model_name ? config->wakenet_model_name : "无");
    s_afe = esp_afe_handle_from_config(config);
    s_afe_data = s_afe ? s_afe->create_from_config(config) : NULL;
    afe_config_free(config);
    if (!s_afe_data) return ESP_FAIL;
    /* Keep model defaults; do not mask capture errors with a lowered threshold. */
    if (xTaskCreatePinnedToCore(audio_capture_task, "audio_capture_task", 6144, NULL, 6, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(esp_sr_task, "esp_sr_task", 10240, NULL, 7, NULL, 1) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}

bool sr_engine_take_utterance(speech_utterance_t *utterance, uint32_t timeout_ms)
{
    return utterance && xQueueReceive(s_utterances, utterance, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void sr_engine_release_utterance(speech_utterance_t *utterance)
{
    if (utterance) {
        free(utterance->samples);
        utterance->samples = NULL;
        utterance->sample_count = 0;
    }
}
