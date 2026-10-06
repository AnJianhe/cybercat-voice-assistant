#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t audio_capture_init(void);
esp_err_t audio_capture_read(int16_t *mono, size_t samples, size_t *samples_read, uint32_t timeout_ms);

