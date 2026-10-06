#include "wake_led.h"
#include "wake_led_config.h"
#include "app_config.h"
#include "uart_link.h"
#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    LED_BOOT_OFF,
    LED_IDLE_BLUE,
    LED_RECOGNIZING_YELLOW,
    LED_STT_CYAN,
    LED_AI_PURPLE,
    LED_TTS_GREEN,
    LED_SOLID_RED,
    LED_BACKEND_BLINK,
    LED_FIXED_PINK,
} led_mode_t;

static const char *TAG = "状态灯";
static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;
static TaskHandle_t s_worker;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static led_mode_t s_mode = LED_BOOT_OFF;
static bool s_ready, s_active;
static int64_t s_conversation, s_pattern_started_ms;
static uint32_t s_generation;
static rmt_symbol_word_t s_symbols[25];

static const char *mode_status(led_mode_t mode)
{
    switch (mode) {
        case LED_IDLE_BLUE: return "idle_blue";
        case LED_RECOGNIZING_YELLOW: return "recognizing_yellow";
        case LED_STT_CYAN: return "speech_to_text_cyan";
        case LED_AI_PURPLE: return "ai_reply_purple";
        case LED_TTS_GREEN: return "text_to_speech_green";
        case LED_SOLID_RED: return "recognition_red";
        case LED_BACKEND_BLINK: return "backend_error_blink";
        case LED_FIXED_PINK: return "fixed_pink";
        default: return "boot_off";
    }
}

static TickType_t wait_ticks(int64_t milliseconds)
{
    if (milliseconds <= 0) return 1;
    TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    return ticks ? ticks : 1;
}

static esp_err_t send_color(uint8_t red, uint8_t green, uint8_t blue)
{
    const uint8_t bytes[] = {green, red, blue};
    size_t symbol = 0;
    for (size_t byte = 0; byte < sizeof(bytes); ++byte) {
        for (int bit = 7; bit >= 0; --bit) {
            bool one = (bytes[byte] & (1U << bit)) != 0;
            s_symbols[symbol++] = (rmt_symbol_word_t) {
                .level0 = 1, .duration0 = one ? 9 : 3,
                .level1 = 0, .duration1 = one ? 3 : 9,
            };
        }
    }
    s_symbols[symbol] = (rmt_symbol_word_t) {
        .level0 = 0, .duration0 = 1500, .level1 = 0, .duration1 = 1500,
    };
    rmt_transmit_config_t config = {.loop_count = 0, .flags.eot_level = 0};
    esp_err_t err = rmt_transmit(s_channel, s_encoder, s_symbols, sizeof(s_symbols), &config);
    if (err == ESP_OK) err = rmt_tx_wait_all_done(s_channel, 100);
    return err;
}

static void finish_pattern_if_current(uint32_t generation)
{
    bool changed = false;
    portENTER_CRITICAL(&s_lock);
    if (s_generation == generation &&
        (s_mode == LED_SOLID_RED || s_mode == LED_BACKEND_BLINK || s_mode == LED_FIXED_PINK)) {
        s_mode = s_ready ? LED_IDLE_BLUE : LED_BOOT_OFF;
        ++s_generation;
        changed = true;
    }
    portEXIT_CRITICAL(&s_lock);
    if (changed) xTaskNotifyGive(s_worker);
}

static void led_worker(void *arg)
{
    (void)arg;
    bool failed = false;
    led_mode_t last_reported_mode = (led_mode_t)-1;
    for (;;) {
        led_mode_t mode;
        int64_t conversation, pattern_started;
        uint32_t generation;
        portENTER_CRITICAL(&s_lock);
        mode = s_mode;
        conversation = s_conversation;
        pattern_started = s_pattern_started_ms;
        generation = s_generation;
        portEXIT_CRITICAL(&s_lock);

        int64_t elapsed = esp_timer_get_time() / 1000 - pattern_started;
        uint8_t red = 0, green = 0, blue = 0;
        const char *state = "boot_off";
        TickType_t wait = portMAX_DELAY;
        if (mode == LED_IDLE_BLUE) {
            blue = WAKE_LED_IDLE_BLUE; state = "idle_blue";
        } else if (mode == LED_RECOGNIZING_YELLOW) {
            red = WAKE_LED_RECOGNIZING_RED;
            green = WAKE_LED_RECOGNIZING_GREEN;
            state = "recognizing_yellow";
        } else if (mode == LED_STT_CYAN) {
            green = WAKE_LED_STT_GREEN;
            blue = WAKE_LED_STT_BLUE;
            state = "speech_to_text_cyan";
        } else if (mode == LED_AI_PURPLE) {
            red = WAKE_LED_AI_RED;
            blue = WAKE_LED_AI_BLUE;
            state = "ai_reply_purple";
        } else if (mode == LED_TTS_GREEN) {
            green = WAKE_LED_TTS_GREEN;
            state = "text_to_speech_green";
        } else if (mode == LED_SOLID_RED) {
            red = WAKE_LED_ERROR_RED; state = "recognition_red";
            wait = wait_ticks(elapsed >= WAKE_LED_PATTERN_MS ? 1 : WAKE_LED_PATTERN_MS - elapsed);
        } else if (mode == LED_FIXED_PINK) {
            red = WAKE_LED_ERROR_RED; blue = WAKE_LED_PINK_BLUE; state = "fixed_pink";
            wait = wait_ticks(elapsed >= WAKE_LED_PATTERN_MS ? 1 : WAKE_LED_PATTERN_MS - elapsed);
        } else if (mode == LED_BACKEND_BLINK) {
            bool show_red = ((elapsed / WAKE_LED_BLINK_MS) & 1) == 0;
            red = show_red ? WAKE_LED_ERROR_RED : 0;
            green = show_red ? 0 : WAKE_LED_ACTIVE_GREEN;
            state = show_red ? "backend_red" : "backend_green";
            int64_t remaining = WAKE_LED_PATTERN_MS - elapsed;
            int64_t phase = WAKE_LED_BLINK_MS - (elapsed % WAKE_LED_BLINK_MS);
            wait = wait_ticks(remaining <= 1 ? 1 : remaining < phase ? remaining : phase);
        }

        if (!failed) {
            esp_err_t err = send_color(red, green, blue);
            if (err != ESP_OK) {
                rmt_disable(s_channel);
                failed = true;
                ESP_LOGE(TAG, "状态灯输出已禁用");
            } else {
                ESP_LOGI(TAG, "状态=%s，GPIO=%d，对话=%lld",
                         state, WAKE_LED_GPIO, (long long)conversation);
                if (mode != last_reported_mode) {
                    if (uart_link_status(mode_status(mode)) == ESP_OK) last_reported_mode = mode;
                }
            }
        }
        if (ulTaskNotifyTake(pdTRUE, wait) == 0 && wait != portMAX_DELAY) {
            int64_t now_elapsed = esp_timer_get_time() / 1000 - pattern_started;
            if (now_elapsed >= WAKE_LED_PATTERN_MS) finish_pattern_if_current(generation);
        }
    }
}

esp_err_t wake_led_init(void)
{
    if (WAKE_LED_GPIO < 0) return ESP_OK;
    if ((WAKE_LED_GPIO != 48 && WAKE_LED_GPIO != 38) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(WAKE_LED_GPIO) ||
        WAKE_LED_GPIO == MIC_I2S_BCLK_GPIO || WAKE_LED_GPIO == MIC_I2S_WS_GPIO ||
        WAKE_LED_GPIO == MIC_I2S_DATA_IN_GPIO || WAKE_LED_GPIO == BRIDGE_UART_TX_GPIO ||
        WAKE_LED_GPIO == BRIDGE_UART_RX_GPIO) return ESP_ERR_INVALID_ARG;
    rmt_tx_channel_config_t config = {
        .gpio_num = WAKE_LED_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    esp_err_t err = rmt_new_tx_channel(&config, &s_channel);
    if (err != ESP_OK) return err;
    rmt_copy_encoder_config_t encoder_config = {};
    err = rmt_new_copy_encoder(&encoder_config, &s_encoder);
    if (err != ESP_OK) {
        rmt_del_channel(s_channel); s_channel = NULL;
        return err;
    }
    err = rmt_enable(s_channel);
    if (err != ESP_OK) {
        rmt_del_encoder(s_encoder); rmt_del_channel(s_channel);
        s_encoder = NULL; s_channel = NULL;
        return err;
    }
    if (xTaskCreate(led_worker, "wake_led", 3072, NULL, 3, &s_worker) != pdPASS) {
        rmt_disable(s_channel); rmt_del_encoder(s_encoder); rmt_del_channel(s_channel);
        s_encoder = NULL; s_channel = NULL; s_worker = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void wake_led_ready(void)
{
    if (!s_worker) return;
    portENTER_CRITICAL(&s_lock);
    s_ready = true;
    s_mode = LED_IDLE_BLUE;
    ++s_generation;
    portEXIT_CRITICAL(&s_lock);
    xTaskNotifyGive(s_worker);
}

void wake_led_listening(int64_t conversation_id)
{
    if (!s_worker) return;
    portENTER_CRITICAL(&s_lock);
    /* A fresh wake word always owns the LED, even while an older cloud job runs. */
    s_conversation = conversation_id;
    s_active = true;
    s_mode = LED_RECOGNIZING_YELLOW;
    ++s_generation;
    portEXIT_CRITICAL(&s_lock);
    xTaskNotifyGive(s_worker);
}

void wake_led_stage(int64_t conversation_id, wake_led_stage_t stage)
{
    if (!s_worker) return;
    led_mode_t mode = LED_RECOGNIZING_YELLOW;
    if (stage == WAKE_LED_STAGE_SPEECH_TO_TEXT) mode = LED_STT_CYAN;
    else if (stage == WAKE_LED_STAGE_AI_REPLY) mode = LED_AI_PURPLE;
    else if (stage == WAKE_LED_STAGE_TEXT_TO_SPEECH) mode = LED_TTS_GREEN;
    bool changed = false;
    portENTER_CRITICAL(&s_lock);
    /* A slow older cloud request must not overwrite a newer conversation's LED. */
    if (!s_active || s_conversation == conversation_id) {
        s_conversation = conversation_id;
        s_active = true;
        s_mode = mode;
        ++s_generation;
        changed = true;
    }
    portEXIT_CRITICAL(&s_lock);
    if (changed) xTaskNotifyGive(s_worker);
}

void wake_led_finished(int64_t conversation_id, wake_led_result_t result)
{
    if (!s_worker) return;
    bool changed = false;
    portENTER_CRITICAL(&s_lock);
    if (s_active && conversation_id == s_conversation) {
        s_active = false;
        if (result == WAKE_LED_SUCCESS) s_mode = LED_IDLE_BLUE;
        else if (result == WAKE_LED_BACKEND_FAILED) s_mode = LED_BACKEND_BLINK;
        else if (result == WAKE_LED_FIXED_COMMAND) s_mode = LED_FIXED_PINK;
        else s_mode = LED_SOLID_RED;
        s_pattern_started_ms = esp_timer_get_time() / 1000;
        ++s_generation;
        changed = true;
    }
    portEXIT_CRITICAL(&s_lock);
    if (changed) xTaskNotifyGive(s_worker);
}

static void start_test_pattern(led_mode_t mode)
{
    portENTER_CRITICAL(&s_lock);
    s_active = false;
    s_mode = mode;
    s_conversation = -1;
    s_pattern_started_ms = esp_timer_get_time() / 1000;
    ++s_generation;
    portEXIT_CRITICAL(&s_lock);
    xTaskNotifyGive(s_worker);
}

bool wake_led_command(const char *command)
{
    led_mode_t mode;
    if (!s_worker) return false;
    if (strcmp(command, "LED_TEST_RED") == 0) mode = LED_SOLID_RED;
    else if (strcmp(command, "LED_TEST_BACKEND") == 0) mode = LED_BACKEND_BLINK;
    else if (strcmp(command, "LED_TEST_PINK") == 0) mode = LED_FIXED_PINK;
    else return false;
    start_test_pattern(mode);
    printf("LED_TEST_ACCEPTED %s duration_ms=%d\n", command, WAKE_LED_PATTERN_MS);
    fflush(stdout);
    return true;
}
