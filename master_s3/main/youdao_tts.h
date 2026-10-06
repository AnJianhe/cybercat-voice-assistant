#pragma once

#include "esp_err.h"
#include <stdbool.h>

/* 有道 WAV/PCM 输出：WROOM 蓝牙或 S3 直连 I2S 喇叭。 */
esp_err_t youdao_tts_stream_to_bridge(const char *text);
esp_err_t youdao_tts_output_init(void);
bool youdao_tts_speaker_mode(void);
esp_err_t youdao_tts_set_speaker_mode(bool speaker);
void youdao_tts_cancel(void);
