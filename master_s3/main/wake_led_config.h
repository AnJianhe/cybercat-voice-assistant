#pragma once
/* Candidate wiring from the user's pasted DevKitC example, NOT autodetected.
 * User confirmed GPIO48 has no other peripherals connected (2026-09-03).
 * RGB pin/type still require visual testing; do not auto-probe other pins.
 * Set -1 to disable. Never use Arduino's virtual RGB_BUILTIN value 97 here. */
#define WAKE_LED_GPIO 48
#define WAKE_LED_IDLE_BLUE 8
#define WAKE_LED_RECOGNIZING_RED 20
#define WAKE_LED_RECOGNIZING_GREEN 16
#define WAKE_LED_STT_GREEN 16
#define WAKE_LED_STT_BLUE 16
#define WAKE_LED_AI_RED 16
#define WAKE_LED_AI_BLUE 20
#define WAKE_LED_TTS_GREEN 20
#define WAKE_LED_ACTIVE_GREEN WAKE_LED_TTS_GREEN
#define WAKE_LED_ERROR_RED 20
#define WAKE_LED_PINK_BLUE 10
#define WAKE_LED_PATTERN_MS 3000
#define WAKE_LED_BLINK_MS 250
