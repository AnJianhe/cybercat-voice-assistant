#pragma once
#include "sr_engine.h"

typedef enum {
    YOUDAO_ASR_OK,
    YOUDAO_ASR_NO_RECOGNITION,
    YOUDAO_ASR_BACKEND_ERROR,
} youdao_asr_status_t;

/* Caller owns the returned UTF-8 text and must free it. */
char *youdao_asr_transcribe(const speech_utterance_t *utterance, youdao_asr_status_t *status);
