#include "frame_protocol.h"
#include <cstring>

uint16_t crc16Update(uint16_t crc, uint8_t byte) {
    crc ^= static_cast<uint16_t>(byte) << 8;
    for (int i = 0; i < 8; ++i) crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    return crc;
}

void FrameParser::reset() { state_ = MAGIC_AA; index_ = 0; runningCrc_ = 0xFFFF; receivedCrc_ = 0; }

bool FrameParser::push(uint8_t b, UartFrame &out, bool &crcError) {
    crcError = false;
    switch (state_) {
    case MAGIC_AA: if (b == 0xAA) state_ = MAGIC_55; break;
    case MAGIC_55: state_ = b == 0x55 ? TYPE : (b == 0xAA ? MAGIC_55 : MAGIC_AA); break;
    case TYPE: frame_.type = static_cast<FrameType>(b); runningCrc_ = crc16Update(0xFFFF, b); state_ = LEN_H; break;
    case LEN_H: frame_.length = static_cast<uint16_t>(b) << 8; runningCrc_ = crc16Update(runningCrc_, b); state_ = LEN_L; break;
    case LEN_L:
        frame_.length |= b; runningCrc_ = crc16Update(runningCrc_, b); index_ = 0;
        if (frame_.length > FRAME_MAX_PAYLOAD) reset(); else state_ = frame_.length ? PAYLOAD : CRC_H;
        break;
    case PAYLOAD:
        frame_.payload[index_++] = b; runningCrc_ = crc16Update(runningCrc_, b);
        if (index_ == frame_.length) state_ = CRC_H;
        break;
    case CRC_H: receivedCrc_ = static_cast<uint16_t>(b) << 8; state_ = CRC_L; break;
    case CRC_L: {
        receivedCrc_ |= b; bool ok = receivedCrc_ == runningCrc_;
        if (ok) { out = frame_; out.payload[out.length] = 0; } else crcError = true;
        reset(); return ok;
    }}
    return false;
}

size_t encodeFrame(FrameType type, const void *payload, uint16_t length, uint8_t *out, size_t capacity) {
    size_t total = static_cast<size_t>(length) + 7;
    if (!out || length > FRAME_MAX_PAYLOAD || capacity < total) return 0;
    out[0] = 0xAA; out[1] = 0x55; out[2] = type; out[3] = length >> 8; out[4] = length;
    if (length && payload) memcpy(out + 5, payload, length);
    uint16_t crc = 0xFFFF;
    for (size_t i = 2; i < static_cast<size_t>(length) + 5; ++i) crc = crc16Update(crc, out[i]);
    out[5 + length] = crc >> 8; out[6 + length] = crc;
    return total;
}
