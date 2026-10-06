#include "frame_protocol.h"
#include <string.h>

uint16_t frame_crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; ++i) {
        crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

void frame_parser_init(frame_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->state = FP_MAGIC_AA;
}

bool frame_parser_push(frame_parser_t *p, uint8_t b, frame_t *out, bool *crc_error)
{
    if (crc_error) *crc_error = false;
    switch (p->state) {
    case FP_MAGIC_AA:
        if (b == 0xAA) p->state = FP_MAGIC_55;
        break;
    case FP_MAGIC_55:
        if (b == 0x55) p->state = FP_TYPE;
        else p->state = (b == 0xAA) ? FP_MAGIC_55 : FP_MAGIC_AA;
        break;
    case FP_TYPE:
        p->frame.type = (frame_type_t)b;
        p->running_crc = frame_crc16_update(0xFFFF, b);
        p->state = FP_LEN_H;
        break;
    case FP_LEN_H:
        p->frame.length = (uint16_t)b << 8;
        p->running_crc = frame_crc16_update(p->running_crc, b);
        p->state = FP_LEN_L;
        break;
    case FP_LEN_L:
        p->frame.length |= b;
        p->running_crc = frame_crc16_update(p->running_crc, b);
        p->index = 0;
        if (p->frame.length > FRAME_MAX_PAYLOAD) {
            frame_parser_init(p);
        } else {
            p->state = p->frame.length ? FP_PAYLOAD : FP_CRC_H;
        }
        break;
    case FP_PAYLOAD:
        p->frame.payload[p->index++] = b;
        p->running_crc = frame_crc16_update(p->running_crc, b);
        if (p->index == p->frame.length) p->state = FP_CRC_H;
        break;
    case FP_CRC_H:
        p->received_crc = (uint16_t)b << 8;
        p->state = FP_CRC_L;
        break;
    case FP_CRC_L: {
        p->received_crc |= b;
        bool ok = p->received_crc == p->running_crc;
        if (ok && out) *out = p->frame;
        if (!ok && crc_error) *crc_error = true;
        frame_parser_init(p);
        return ok;
    }
    }
    return false;
}

size_t frame_encode(frame_type_t type, const void *payload, uint16_t length,
                    uint8_t *out, size_t capacity)
{
    size_t total = (size_t)length + 7;
    if (!out || length > FRAME_MAX_PAYLOAD || capacity < total) return 0;
    out[0] = 0xAA; out[1] = 0x55; out[2] = (uint8_t)type;
    out[3] = (uint8_t)(length >> 8); out[4] = (uint8_t)length;
    if (length && payload) memcpy(out + 5, payload, length);
    uint16_t crc = 0xFFFF;
    for (size_t i = 2; i < (size_t)length + 5; ++i) crc = frame_crc16_update(crc, out[i]);
    out[5 + length] = (uint8_t)(crc >> 8);
    out[6 + length] = (uint8_t)crc;
    return total;
}
