#pragma once

#include "esp_err.h"
#include "frame_protocol.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t uart_link_init(void);
esp_err_t uart_link_send(frame_type_t type, const void *payload, uint16_t length);
esp_err_t uart_link_command(const char *command);
esp_err_t uart_link_stop(void);
esp_err_t uart_link_pcm_start(uint32_t sample_rate, uint8_t bits, uint8_t channels);
esp_err_t uart_link_pcm_data(const uint8_t *data, size_t length);
esp_err_t uart_link_pcm_end(void);
esp_err_t uart_link_tts_text(const char *text);
esp_err_t uart_link_mp3_start(uint32_t total_bytes);
esp_err_t uart_link_mp3_data(const uint8_t *data, size_t length);
esp_err_t uart_link_mp3_end(void);
esp_err_t uart_link_status(const char *status);
/* 诊断：连续发送 N 个 1024 字节帧，测量链路实际吞吐。 */
void uart_link_selftest(int chunks);
