"""Host-side protocol vectors. Run: python tests/test_protocol.py"""

MAGIC = bytes((0xAA, 0x55))


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode(frame_type: int, payload: bytes = b"") -> bytes:
    header = bytes((frame_type, len(payload) >> 8, len(payload) & 0xFF))
    body = header + payload
    crc = crc16_ccitt_false(body)
    return MAGIC + body + crc.to_bytes(2, "big")


def decode(frame: bytes) -> tuple[int, bytes]:
    assert frame[:2] == MAGIC
    length = int.from_bytes(frame[3:5], "big")
    assert len(frame) == length + 7
    assert crc16_ccitt_false(frame[2:-2]) == int.from_bytes(frame[-2:], "big")
    return frame[2], frame[5:-2]


def main() -> None:
    assert crc16_ccitt_false(b"123456789") == 0x29B1
    vectors = [
        (0x01, b"PLAY:/music/song001.mp3"),
        (0x02, bytes((0, 0, 0x3E, 0x80, 16, 1, 0))),
        (0x03, bytes(range(256)) * 4),
        (0x04, b""),
        (0x05, b""),
        (0x06, b"OK:STOPPED"),
        (0x07, b"ERR:CRC"),
        (0x08, "你好，RS106".encode("utf-8")),
        (0x09, b"idle_blue"),
    ]
    for expected_type, expected_payload in vectors:
        assert decode(encode(expected_type, expected_payload)) == (expected_type, expected_payload)
    corrupted = bytearray(encode(0x03, b"pcm"))
    corrupted[5] ^= 1
    try:
        decode(bytes(corrupted))
    except AssertionError:
        pass
    else:
        raise AssertionError("corrupted frame was accepted")
    print(f"protocol vectors passed: {len(vectors)}; CRC check passed")


if __name__ == "__main__":
    main()
