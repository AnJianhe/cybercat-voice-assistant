import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("conversation_logs", ROOT / "conversation_logs.py")
logs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(logs)

def record(seq, question="你是谁？", answer="我是赛博猫猫。", committed=logs.COMMIT, result=0):
    q, a = question.encode(), answer.encode()
    header = logs.HEADER.pack(logs.MAGIC, 1, seq, 1788438000, 100, 800, len(q), len(a),
                              result, zlib.crc32(q + a), 0, committed)
    header = header[:56] + struct.pack("<I", zlib.crc32(header[:56])) + header[60:]
    return (header + q + a).ljust(logs.SLOT_SIZE, b"\xff")

class ConversationLogs(unittest.TestCase):
    def blank(self):
        return bytearray(b"\xff" * (logs.SLOT_COUNT * logs.SLOT_SIZE))

    def test_empty(self):
        self.assertEqual(logs.decode_flash(self.blank()), [])

    def test_utf8_roundtrip(self):
        data = self.blank()
        data[:logs.SLOT_SIZE] = record(1)
        decoded = logs.decode_flash(data)
        self.assertEqual(decoded[0]["question"], "你是谁？")
        self.assertEqual(decoded[0]["answer"], "我是赛博猫猫。")

    def test_appended_results_preserve_old_flash_values(self):
        self.assertEqual(logs.RESULTS[:10], ("ok", "asr_failed", "deepseek_failed",
            "deepseek_partial", "offline", "no_speech", "fixed_command",
            "queue_full", "no_memory", "selftest"))
        data = self.blank()
        data[:logs.SLOT_SIZE] = record(1, result=10)
        data[logs.SLOT_SIZE:2 * logs.SLOT_SIZE] = record(2, result=11)
        self.assertEqual([item["result"] for item in logs.decode_flash(data)],
                         ["no_recognition", "backend_failed"])

    def test_ring_orders_newest_generation(self):
        data = self.blank()
        for slot in range(logs.SLOT_COUNT):
            seq = slot + 1 if slot else 65
            data[slot * logs.SLOT_SIZE:(slot + 1) * logs.SLOT_SIZE] = record(seq)
        seqs = [int(item["sequence"]) for item in logs.decode_flash(data)]
        self.assertEqual(seqs, list(range(2, 66)))

    def test_uncommitted_not_exported(self):
        data = self.blank()
        data[:logs.SLOT_SIZE] = record(1, committed=0xffffffff)
        self.assertEqual(logs.decode_flash(data), [])

    def test_body_corruption_not_exported(self):
        data = self.blank()
        data[:logs.SLOT_SIZE] = record(1)
        data[70] ^= 1
        self.assertEqual(logs.decode_flash(data), [])

    def test_foreign_data_rejected(self):
        data = self.blank()
        data[100] = 0
        with self.assertRaises(ValueError):
            logs.decode_flash(data)

    def test_header_corruption_rejected(self):
        data = self.blank()
        data[:logs.SLOT_SIZE] = record(1)
        data[40] ^= 1
        with self.assertRaises(ValueError):
            logs.decode_flash(data)

    def test_truncated_flash(self):
        with self.assertRaises(ValueError):
            logs.decode_flash(b"short")

    def test_serial_missing_end(self):
        with self.assertRaises(TimeoutError):
            logs.collect_dump(["CHATLOG_BEGIN count=1"])

    def test_serial_count_mismatch(self):
        with self.assertRaises(ValueError):
            logs.collect_dump(["CHATLOG_BEGIN count=1", "CHATLOG_END count=0"])

    def test_empty_serial_dump(self):
        self.assertEqual(logs.collect_dump(["CHATLOG_BEGIN count=0", "CHATLOG_END count=0"]), [])

if __name__ == "__main__":
    unittest.main()
