#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WAKE_LED_SUCCESS,
    WAKE_LED_RECOGNITION_FAILED,
    WAKE_LED_BACKEND_FAILED,
    WAKE_LED_REPLY_FAILED,
    WAKE_LED_FIXED_COMMAND,
} wake_led_result_t;

typedef enum {
    WAKE_LED_STAGE_RECOGNIZING,
    WAKE_LED_STAGE_SPEECH_TO_TEXT,
    WAKE_LED_STAGE_AI_REPLY,
    WAKE_LED_STAGE_TEXT_TO_SPEECH,
} wake_led_stage_t;

esp_err_t wake_led_init(void);
void wake_led_ready(void);
void wake_led_listening(int64_t conversation_id);
void wake_led_stage(int64_t conversation_id, wake_led_stage_t stage);
void wake_led_finished(int64_t conversation_id, wake_led_result_t result);
bool wake_led_command(const char *command);
