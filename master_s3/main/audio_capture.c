#include "audio_capture.h"
#include "app_config.h"

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdlib.h>

static const char *TAG = "音频采集";
static i2s_chan_handle_t s_rx;
static int32_t *s_raw;
static size_t s_raw_capacity;

esp_err_t audio_capture_init(void)
{
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel, NULL, &s_rx), TAG, "new channel");
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = MIC_I2S_BCLK_GPIO,
            .ws = MIC_I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = MIC_I2S_DATA_IN_GPIO,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    /* On ESP32-S3 the helper defaults to BOTH even in MONO mode.
     * Select only the wired microphone slot; otherwise inactive-slot zeros
     * are interleaved into the 16 kHz stream sent to the speech models. */
    config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &config), TAG, "std mode");
    ESP_LOGI(TAG, "I2S 输入：16000 Hz、32 位 Philips、左声道单麦克风");
    return i2s_channel_enable(s_rx);
}

esp_err_t audio_capture_read(int16_t *mono, size_t samples, size_t *samples_read, uint32_t timeout_ms)
{
    if (!mono || !samples_read) return ESP_ERR_INVALID_ARG;
    if (samples > s_raw_capacity) {
        int32_t *grown = realloc(s_raw, samples * sizeof(int32_t));
        if (!grown) return ESP_ERR_NO_MEM;
        s_raw = grown;
        s_raw_capacity = samples;
    }
    size_t bytes = 0;
    esp_err_t err = i2s_channel_read(s_rx, s_raw, samples * sizeof(int32_t), &bytes,
                                     timeout_ms);
    size_t count = bytes / sizeof(int32_t);
    for (size_t i = 0; i < count; ++i) {
        int32_t value = s_raw[i] >> MIC_SAMPLE_RIGHT_SHIFT;
        if (value > INT16_MAX) value = INT16_MAX;
        if (value < INT16_MIN) value = INT16_MIN;
        mono[i] = (int16_t)value;
    }
    *samples_read = count;
    return err;
}
