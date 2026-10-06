#pragma once

#include "esp_err.h"
#include "conversation_log.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t *samples;
    size_t sample_count;
    conversation_stamp_t log_stamp;
} speech_utterance_t;

esp_err_t sr_engine_init(void);
bool sr_engine_take_utterance(speech_utterance_t *utterance, uint32_t timeout_ms);
void sr_engine_release_utterance(speech_utterance_t *utterance);
