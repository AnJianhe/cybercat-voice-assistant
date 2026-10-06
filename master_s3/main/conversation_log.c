#include "conversation_log.h"
#include "cloud_service.h"
#include "espnow_control.h"
#include "wake_led.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_MAGIC UINT32_C(0x31474c43)
#define LOG_COMMIT UINT32_C(0xc0dec0de)
#define LOG_VERSION 1U
#define SLOT_SIZE 16384U
#define SLOT_COUNT 64U
#define QUESTION_MAX 2048U
#define ANSWER_MAX 8192U
/* Uses only the first 1 MiB of the pre-existing 2 MiB storage partition.
 * Never mount this reserved region as SPIFFS. Unknown data is never formatted. */
typedef struct {
    uint32_t magic, version;
    uint64_t sequence;
    int64_t unix_time, started_ms, ended_ms;
    uint32_t question_len, answer_len, result, body_crc, header_crc, committed;
} record_header_t;
_Static_assert(sizeof(record_header_t) == 64, "Flash format size");
_Static_assert(offsetof(record_header_t, header_crc) == 56, "Flash CRC offset");
_Static_assert(offsetof(record_header_t, committed) == 60, "Flash commit offset");

typedef enum { JOB_SAVE, JOB_DUMP, JOB_STATUS } job_type_t;
typedef struct {
    job_type_t type;
    record_header_t header;
    char *question, *answer;
} log_job_t;

static const char *TAG = "对话记录";
static const esp_partition_t *s_partition;
static QueueHandle_t s_jobs;
static uint64_t s_sequence;
static unsigned s_next_slot, s_count;
static uint64_t s_slot_sequences[SLOT_COUNT];
static bool s_writable;

static const char *result_name(uint32_t result)
{
    static const char *names[] = {"ok", "asr_failed", "deepseek_failed",
        "deepseek_partial", "offline", "no_speech", "fixed_command",
        "queue_full", "no_memory", "selftest", "no_recognition", "backend_failed"};
    return result < sizeof(names) / sizeof(names[0]) ? names[result] : "unknown";
}

static bool all_erased(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    for (size_t i = 0; i < size; ++i) if (bytes[i] != 0xff) return false;
    return true;
}

static bool header_valid(const record_header_t *header)
{
    return header->magic == LOG_MAGIC && header->version == LOG_VERSION &&
        header->sequence != 0 && header->sequence != UINT64_MAX &&
        header->question_len <= QUESTION_MAX && header->answer_len <= ANSWER_MAX &&
        header->result <= CHATLOG_BACKEND_FAILED &&
        header->header_crc == esp_rom_crc32_le(0, (const uint8_t *)header,
                                               offsetof(record_header_t, header_crc));
}

static esp_err_t body_crc(unsigned slot, const record_header_t *header, uint32_t *crc)
{
    uint8_t buffer[512];
    size_t remaining = header->question_len + header->answer_len;
    size_t offset = slot * SLOT_SIZE + sizeof(*header);
    *crc = 0;
    while (remaining) {
        size_t part = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        esp_err_t err = esp_partition_read(s_partition, offset, buffer, part);
        if (err != ESP_OK) return err;
        *crc = esp_rom_crc32_le(*crc, buffer, part);
        offset += part;
        remaining -= part;
    }
    return ESP_OK;
}

static esp_err_t scan_storage(void)
{
    uint8_t buffer[512];
    bool ambiguous = false;
    for (unsigned slot = 0; slot < SLOT_COUNT; ++slot) {
        record_header_t header;
        esp_err_t err = esp_partition_read(s_partition, slot * SLOT_SIZE, &header, sizeof(header));
        if (err != ESP_OK) return err;
        if (all_erased(&header, sizeof(header))) {
            /* Check the entire supposedly empty slot, not just its header. */
            for (size_t offset = sizeof(header); offset < SLOT_SIZE;) {
                size_t part = SLOT_SIZE - offset;
                if (part > sizeof(buffer)) part = sizeof(buffer);
                err = esp_partition_read(s_partition, slot * SLOT_SIZE + offset, buffer, part);
                if (err != ESP_OK) return err;
                if (!all_erased(buffer, part)) { ambiguous = true; break; }
                offset += part;
            }
            continue;
        }
        if (!header_valid(&header)) {
            /* Conservative recovery: never erase foreign data or an ambiguous header. */
            ambiguous = true;
            continue;
        }
        if (header.sequence >= s_sequence) {
            s_sequence = header.sequence;
            s_next_slot = (slot + 1) % SLOT_COUNT;
        }
        uint32_t crc;
        if (header.committed == LOG_COMMIT &&
            body_crc(slot, &header, &crc) == ESP_OK && crc == header.body_crc) {
            s_slot_sequences[slot] = header.sequence;
            ++s_count;
        }
    }
    /* Keep readable older records available after an interrupted header write.
     * Ambiguous/foreign bytes disable all further erases, not read-only export. */
    s_writable = !ambiguous;
    if (ambiguous) ESP_LOGW(TAG, "对话记录处于只读保护状态：存储内容不明确，未擦除任何数据");
    return ESP_OK;
}

static char *copy_text(const char *text, size_t limit)
{
    if (!text) text = "";
    size_t length = strnlen(text, limit + 1);
    if (length > limit) {
        length = limit;
        /* Do not cut a UTF-8 codepoint between its leading/continuation bytes. */
        while (length && (((unsigned char)text[length] & 0xc0) == 0x80)) --length;
    }
    char *copy = heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) copy = malloc(length + 1);
    if (copy) { memcpy(copy, text, length); copy[length] = 0; }
    return copy;
}

static esp_err_t save_record(log_job_t *job)
{
    if (!s_writable || s_sequence >= UINT64_MAX - 1) return ESP_ERR_INVALID_STATE;
    record_header_t *header = &job->header;
    header->magic = LOG_MAGIC;
    header->version = LOG_VERSION;
    header->sequence = ++s_sequence;
    header->body_crc = esp_rom_crc32_le(0, (const uint8_t *)job->question, header->question_len);
    header->body_crc = esp_rom_crc32_le(header->body_crc,
                                       (const uint8_t *)job->answer, header->answer_len);
    header->header_crc = esp_rom_crc32_le(0, (const uint8_t *)header,
                                         offsetof(record_header_t, header_crc));
    header->committed = UINT32_MAX;
    unsigned slot = s_next_slot;
    size_t offset = slot * SLOT_SIZE;
    esp_err_t err = esp_partition_erase_range(s_partition, offset, SLOT_SIZE);
    if (s_slot_sequences[slot]) {
        s_slot_sequences[slot] = 0;
        --s_count;
    }
    if (err == ESP_OK) err = esp_partition_write(s_partition, offset, header, sizeof(*header));
    if (err == ESP_OK && header->question_len)
        err = esp_partition_write(s_partition, offset + sizeof(*header),
                                  job->question, header->question_len);
    if (err == ESP_OK && header->answer_len)
        err = esp_partition_write(s_partition, offset + sizeof(*header) + header->question_len,
                                  job->answer, header->answer_len);
    uint32_t verified_crc = 0;
    if (err == ESP_OK) err = body_crc(slot, header, &verified_crc);
    if (err == ESP_OK && verified_crc != header->body_crc) err = ESP_ERR_INVALID_CRC;
    uint32_t committed = LOG_COMMIT;
    if (err == ESP_OK)
        err = esp_partition_write(s_partition, offset + offsetof(record_header_t, committed),
                                  &committed, sizeof(committed));
    record_header_t verify;
    if (err == ESP_OK) err = esp_partition_read(s_partition, offset, &verify, sizeof(verify));
    if (err == ESP_OK && (!header_valid(&verify) || verify.committed != LOG_COMMIT))
        err = ESP_ERR_INVALID_CRC;
    if (err != ESP_OK) {
        /* Avoid repeatedly erasing on a failing storage device. */
        s_writable = false;
        return err;
    }
    if (!s_slot_sequences[slot]) ++s_count;
    s_slot_sequences[slot] = header->sequence;
    s_next_slot = (slot + 1) % SLOT_COUNT;
    ESP_LOGI(TAG, "对话记录已保存：序号=%" PRIu64 "，结果=%s，录音已停止",
             header->sequence, result_name(header->result));
    return ESP_OK;
}

static void dump_records(void)
{
    printf("CHATLOG_BEGIN count=%u\n", s_count);
    unsigned emitted = 0;
    for (unsigned step = 0; step < SLOT_COUNT; ++step) {
        unsigned slot = (s_next_slot + step) % SLOT_COUNT;
        if (!s_slot_sequences[slot]) continue;
        record_header_t header;
        uint32_t crc;
        if (esp_partition_read(s_partition, slot * SLOT_SIZE, &header, sizeof(header)) != ESP_OK ||
            !header_valid(&header) || header.committed != LOG_COMMIT ||
            body_crc(slot, &header, &crc) != ESP_OK || crc != header.body_crc) {
            printf("CHATLOG_ERROR invalid_record slot=%u\n", slot);
            continue;
        }
        char *question = malloc(header.question_len + 1);
        char *answer = malloc(header.answer_len + 1);
        if (!question || !answer) {
            free(question); free(answer);
            printf("CHATLOG_ERROR no_memory\n");
            break;
        }
        size_t offset = slot * SLOT_SIZE + sizeof(header);
        esp_err_t err = esp_partition_read(s_partition, offset, question, header.question_len);
        if (err == ESP_OK)
            err = esp_partition_read(s_partition, offset + header.question_len, answer, header.answer_len);
        question[header.question_len] = 0;
        answer[header.answer_len] = 0;
        if (err != ESP_OK) {
            free(question); free(answer);
            printf("CHATLOG_ERROR read_failed\n");
            continue;
        }
        char sequence[24];
        snprintf(sequence, sizeof(sequence), "%" PRIu64, header.sequence);
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddStringToObject(root, "sequence", sequence);
            cJSON_AddNumberToObject(root, "unix_time", (double)header.unix_time);
            cJSON_AddNumberToObject(root, "started_ms", (double)header.started_ms);
            cJSON_AddNumberToObject(root, "ended_ms", (double)header.ended_ms);
            cJSON_AddStringToObject(root, "result", result_name(header.result));
            cJSON_AddStringToObject(root, "question", question);
            cJSON_AddStringToObject(root, "answer", answer);
        }
        char *json = root ? cJSON_PrintUnformatted(root) : NULL;
        if (json) {
            printf("CHATLOG_RECORD %s\n", json);
            ++emitted;
        } else printf("CHATLOG_ERROR no_memory\n");
        free(json); cJSON_Delete(root);
        free(question); free(answer);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    printf("CHATLOG_END count=%u\n", emitted);
    fflush(stdout);
}

static void log_worker(void *arg)
{
    (void)arg;
    log_job_t job;
    for (;;) {
        if (xQueueReceive(s_jobs, &job, portMAX_DELAY) != pdTRUE) continue;
        if (job.type == JOB_SAVE) {
            esp_err_t err = save_record(&job);
            if (err != ESP_OK) ESP_LOGE(TAG, "对话记录保存失败");
            free(job.question); free(job.answer);
        } else if (job.type == JOB_DUMP) dump_records();
        else {
            printf("CHATLOG_STATUS count=%u capacity=%u last=%" PRIu64 " writable=%u\n",
                   s_count, SLOT_COUNT, s_sequence, (unsigned)s_writable);
            fflush(stdout);
        }
    }
}

esp_err_t conversation_log_init(void)
{
    s_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                           ESP_PARTITION_SUBTYPE_ANY, "storage");
    if (!s_partition || s_partition->size < SLOT_SIZE * SLOT_COUNT ||
        s_partition->encrypted || SLOT_SIZE % s_partition->erase_size)
        return ESP_ERR_INVALID_STATE;
    esp_err_t err = scan_storage();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "对话记录功能已保护性禁用：存储未识别，未擦除任何数据");
        return err;
    }
    s_jobs = xQueueCreate(4, sizeof(log_job_t));
    if (!s_jobs) return ESP_ERR_NO_MEM;
    if (xTaskCreate(log_worker, "conversation_log", 8192, NULL, 2, NULL) != pdPASS) {
        vQueueDelete(s_jobs); s_jobs = NULL; s_writable = false;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "对话记录已就绪：已保存 %u 条，容量=%u，可写=%u；USB 命令为 LOG_DUMP、LOG_STATUS",
             s_count, SLOT_COUNT, (unsigned)s_writable);
    return ESP_OK;
}

conversation_stamp_t conversation_log_begin(void)
{
    time_t now = time(NULL);
    return (conversation_stamp_t) {
        .started_ms = esp_timer_get_time() / 1000,
        .unix_time = now >= 1704067200 ? (int64_t)now : 0,
    };
}

void conversation_log_finish(conversation_stamp_t stamp, conversation_result_t result,
                             const char *question, const char *answer)
{
    /* End the matching visual cue even if storage is unavailable/full. */
    wake_led_result_t led_result = WAKE_LED_REPLY_FAILED;
    if (result == CHATLOG_OK) led_result = WAKE_LED_SUCCESS;
    else if (result == CHATLOG_FIXED_COMMAND) led_result = WAKE_LED_FIXED_COMMAND;
    else if (result == CHATLOG_OFFLINE || result == CHATLOG_BACKEND_FAILED ||
             result == CHATLOG_DEEPSEEK_PARTIAL) led_result = WAKE_LED_BACKEND_FAILED;
    else if (result == CHATLOG_NO_SPEECH || result == CHATLOG_NO_RECOGNITION ||
             result == CHATLOG_ASR_FAILED) led_result = WAKE_LED_RECOGNITION_FAILED;
    wake_led_finished(stamp.started_ms, led_result);
    if (!s_jobs) return;
    log_job_t job = {
        .type = JOB_SAVE,
        .header = {
            .unix_time = stamp.unix_time,
            .started_ms = stamp.started_ms,
            .ended_ms = esp_timer_get_time() / 1000,
            .result = result,
        },
        .question = copy_text(question, QUESTION_MAX),
        .answer = copy_text(answer, ANSWER_MAX),
    };
    if (job.question && job.answer) {
        job.header.question_len = strlen(job.question);
        job.header.answer_len = strlen(job.answer);
        if (xQueueSend(s_jobs, &job, 0) == pdTRUE) return;
    }
    free(job.question); free(job.answer);
    ESP_LOGW(TAG, "对话记录未保存：队列已满或内存不足");
}

bool conversation_log_command(const char *command)
{
    if (cloud_service_usb_command(command)) return true;
    if (wake_led_command(command)) return true;
    if (strcmp(command, "C3_PING") == 0) {
        esp_err_t err = espnow_control_send("PING");
        printf("C3_PING %s\n", err == ESP_OK ? "QUEUED" : esp_err_to_name(err));
        fflush(stdout);
        return true;
    }
    log_job_t job = {0};
    if (strcmp(command, "LOG_DUMP") == 0) job.type = JOB_DUMP;
    else if (strcmp(command, "LOG_STATUS") == 0) job.type = JOB_STATUS;
    else if (strcmp(command, "LOG_SELFTEST") == 0) {
        if (!s_jobs) printf("CHATLOG_ERROR unavailable\n");
        else conversation_log_finish(conversation_log_begin(), CHATLOG_SELFTEST,
                                     "local storage self-test", "no audio or network request");
        return true;
    } else return false;
    if (!s_jobs || xQueueSend(s_jobs, &job, 0) != pdTRUE)
        printf("CHATLOG_ERROR unavailable_or_busy\n");
    return true;
}
