"""Export the S3's completed conversation records without resetting it."""
import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import struct
import time
import zlib

MAGIC = 0x31474C43
COMMIT = 0xC0DEC0DE
SLOT_SIZE = 16384
SLOT_COUNT = 64
HEADER = struct.Struct("<IIQqqqIIIIII")
RESULTS = ("ok", "asr_failed", "deepseek_failed", "deepseek_partial", "offline",
           "no_speech", "fixed_command", "queue_full", "no_memory", "selftest",
           "no_recognition", "backend_failed")
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")

def decode_flash(data):
    """Read-only decoder for the first 1 MiB of a storage-partition backup."""
    if len(data) < SLOT_SIZE * SLOT_COUNT:
        raise ValueError("truncated Flash backup")
    records = []
    for slot in range(SLOT_COUNT):
        block = data[slot * SLOT_SIZE:(slot + 1) * SLOT_SIZE]
        if block == b"\xff" * SLOT_SIZE:
            continue
        values = HEADER.unpack_from(block)
        magic, version, seq, utc, start, end, qlen, alen, result, crc, hcrc, committed = values
        if (magic != MAGIC or version != 1 or seq in (0, 2**64 - 1)
                or qlen > 2048 or alen > 8192 or result >= len(RESULTS)
                or zlib.crc32(block[:56]) != hcrc):
            raise ValueError(f"unrecognized or damaged header at slot {slot}")
        if committed != COMMIT:
            continue
        body = block[HEADER.size:HEADER.size + qlen + alen]
        if zlib.crc32(body) != crc:
            continue
        records.append({
            "sequence": str(seq), "unix_time": utc, "started_ms": start, "ended_ms": end,
            "result": RESULTS[result],
            "question": body[:qlen].decode("utf-8"),
            "answer": body[qlen:].decode("utf-8"),
        })
    records.sort(key=lambda item: int(item["sequence"]))
    return records

def collect_dump(lines):
    records = []
    expected = None
    for line in lines:
        line = ANSI.sub("", line).strip()
        if "CHATLOG_ERROR" in line:
            raise RuntimeError(line)
        if "CHATLOG_BEGIN count=" in line:
            if expected is not None:
                raise ValueError("duplicate dump begin")
            expected = int(line.split("CHATLOG_BEGIN count=", 1)[1])
        elif "CHATLOG_RECORD " in line and expected is not None:
            item = json.loads(line.split("CHATLOG_RECORD ", 1)[1])
            required = {"sequence", "unix_time", "started_ms", "ended_ms",
                        "result", "question", "answer"}
            if not required.issubset(item):
                raise ValueError("incomplete record")
            if item["result"] not in RESULTS:
                raise ValueError("unknown record result")
            if not isinstance(item["question"], str) or not isinstance(item["answer"], str):
                raise ValueError("invalid text")
            records.append(item)
        elif "CHATLOG_END count=" in line and expected is not None:
            emitted = int(line.split("CHATLOG_END count=", 1)[1])
            if expected != emitted or emitted != len(records):
                raise ValueError("incomplete serial export")
            sequences = [int(item["sequence"]) for item in records]
            if sequences != sorted(set(sequences)):
                raise ValueError("duplicate or unordered records")
            return records
    raise TimeoutError("no complete serial export")

def serial_lines(port, command, timeout):
    import serial
    connection = serial.Serial()
    connection.port = port
    connection.baudrate = 115200
    connection.timeout = 1
    connection.dtr = False
    connection.rts = False
    connection.open()
    try:
        connection.reset_input_buffer()
        connection.write((command + "\n").encode("ascii"))
        connection.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            raw = connection.readline()
            if raw:
                yield raw.decode("utf-8", errors="replace")
    finally:
        connection.close()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", nargs="?", choices=("dump", "status", "selftest"), default="dump")
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--flash-backup", type=Path, help="Decode an existing storage backup; no serial access")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.flash_backup:
        records = decode_flash(args.flash_backup.read_bytes())
    elif args.action == "dump":
        lines = serial_lines(args.port, "LOG_DUMP", 120)
        try:
            records = collect_dump(lines)
        finally:
            lines.close()
    else:
        command = "LOG_STATUS" if args.action == "status" else "LOG_SELFTEST"
        expected = "CHATLOG_STATUS" if args.action == "status" else "CHATLOG_SAVED"
        lines = serial_lines(args.port, command, 20)
        try:
            for line in lines:
                if "CHATLOG_ERROR" in line or "CHATLOG_DROPPED" in line:
                    raise RuntimeError(ANSI.sub("", line).strip())
                if expected in line:
                    print(ANSI.sub("", line).strip())
                    return 0
            raise TimeoutError("S3 did not acknowledge command")
        finally:
            lines.close()
    output = args.output or (Path(__file__).parent / "diagnostics" /
                            ("s3_conversations_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".json"))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(records, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(f"saved {len(records)} records: {output}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
