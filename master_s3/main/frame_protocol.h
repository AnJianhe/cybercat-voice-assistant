#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_MAX_PAYLOAD 1024

typedef enum {
    FRAME_CMD       = 0x01,
    FRAME_PCM_START = 0x02,
    FRAME_PCM_DATA  = 0x03,
    FRAME_PCM_END   = 0x04,
    FRAME_STOP      = 0x05,
    FRAME_ACK       = 0x06,
    FRAME_ERROR     = 0x07,
    FRAME_TTS_TEXT  = 0x08,
    FRAME_STATUS    = 0x09,
    FRAME_MP3_START = 0x0A,
    FRAME_MP3_DATA  = 0x0B,
    FRAME_MP3_END   = 0x0C,
} frame_type_t;

typedef struct {
    frame_type_t type;
    uint16_t length;
    uint8_t payload[FRAME_MAX_PAYLOAD];
} frame_t;

typedef enum {
    FP_MAGIC_AA,
    FP_MAGIC_55,
    FP_TYPE,
    FP_LEN_H,
    FP_LEN_L,
    FP_PAYLOAD,
    FP_CRC_H,
    FP_CRC_L,
} frame_parser_state_t;

typedef struct {
    frame_parser_state_t state;
    frame_t frame;
    uint16_t index;
    uint16_t received_crc;
    uint16_t running_crc;
} frame_parser_t;

uint16_t frame_crc16_update(uint16_t crc, uint8_t byte);
void frame_parser_init(frame_parser_t *parser);
bool frame_parser_push(frame_parser_t *parser, uint8_t byte, frame_t *out, bool *crc_error);
size_t frame_encode(frame_type_t type, const void *payload, uint16_t length,
                    uint8_t *out, size_t out_capacity);
