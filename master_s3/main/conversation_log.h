#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* Metadata only. No audio or global ambient-log hook is retained. */
typedef struct {
    int64_t started_ms;
    int64_t unix_time;
} conversation_stamp_t;

typedef enum {
    CHATLOG_OK = 0,
    CHATLOG_ASR_FAILED,
    CHATLOG_DEEPSEEK_FAILED,
    CHATLOG_DEEPSEEK_PARTIAL,
    CHATLOG_OFFLINE,
    CHATLOG_NO_SPEECH,
    CHATLOG_FIXED_COMMAND,
    CHATLOG_QUEUE_FULL,
    CHATLOG_NO_MEMORY,
    CHATLOG_SELFTEST,
    /* Appended: existing on-Flash numeric values 0..9 must never change. */
    CHATLOG_NO_RECOGNITION,
    CHATLOG_BACKEND_FAILED,
} conversation_result_t;

esp_err_t conversation_log_init(void);
conversation_stamp_t conversation_log_begin(void);
/* Seals an immutable record at this call, queued for a single Flash commit. */
void conversation_log_finish(conversation_stamp_t stamp, conversation_result_t result,
                             const char *question, const char *answer);
/* Nonblocking USB command dispatch; export runs on the storage worker. */
bool conversation_log_command(const char *command);
