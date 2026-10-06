#pragma once
#include <Arduino.h>
#include "config.h"

enum FrameType : uint8_t {
    FRAME_CMD = 0x01, FRAME_PCM_START = 0x02, FRAME_PCM_DATA = 0x03,
    FRAME_PCM_END = 0x04, FRAME_STOP = 0x05, FRAME_ACK = 0x06, FRAME_ERROR = 0x07,
    FRAME_TTS_TEXT = 0x08, FRAME_STATUS = 0x09,
    FRAME_MP3_START = 0x0A, FRAME_MP3_DATA = 0x0B, FRAME_MP3_END = 0x0C,
};

struct UartFrame { FrameType type; uint16_t length; uint8_t payload[FRAME_MAX_PAYLOAD + 1]; };

class FrameParser {
public:
    FrameParser() { reset(); }
    bool push(uint8_t byte, UartFrame &out, bool &crcError);
    void reset();
private:
    enum State { MAGIC_AA, MAGIC_55, TYPE, LEN_H, LEN_L, PAYLOAD, CRC_H, CRC_L } state_;
    UartFrame frame_{};
    uint16_t index_ = 0, runningCrc_ = 0xFFFF, receivedCrc_ = 0;
};

uint16_t crc16Update(uint16_t crc, uint8_t byte);
size_t encodeFrame(FrameType type, const void *payload, uint16_t length, uint8_t *out, size_t capacity);
