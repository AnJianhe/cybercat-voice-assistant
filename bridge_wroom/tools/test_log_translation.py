import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cybercat_bridge_config import (  # noqa: E402
    _ASR_PHASE_RE,
    BridgeConfigurator,
    _LATENCY_RE,
    _UTTERANCE_LATENCY_RE,
    format_latency_summary,
    translate_log_line,
)


class LogTranslationTests(unittest.TestCase):
    def test_bluetooth_vendor_message_is_translated(self):
        line = "[I][BluetoothA2DPSource.cpp:359] app_gap_callback(): [BT_AV] Device discovery failed, continue to discover..."
        translated = translate_log_line(line)
        self.assertIn("[蓝牙音频]", translated)
        self.assertIn("本轮蓝牙扫描失败，继续扫描", translated)
        self.assertNotIn("Device discovery failed", translated)

    def test_connection_state_is_translated_but_event_code_is_kept(self):
        translated = translate_log_line("[BT_AV] bt_app_av_sm_hdlr() state CONNECTING, evt 0xff00")
        self.assertIn("状态：正在连接", translated)
        self.assertIn("0xff00", translated)
        self.assertNotIn("bt_app_av_sm_hdlr", translated)

    def test_known_network_failure_is_translated(self):
        translated = translate_log_line("HTTPClient: error(-1): connection refused")
        self.assertEqual(translated, "HTTP 请求错误(-1): 连接被拒绝")

    def test_esp_rom_boot_fields_are_translated(self):
        translated = translate_log_line(
            "ESP-ROM:esp32s3-20210327 rst:0x15 boot:0x2a"
        )
        self.assertIn("芯片启动 ROM：esp32s3-20210327", translated)
        self.assertIn("复位原因：0x15", translated)
        self.assertIn("启动方式：0x2a", translated)

    def test_wifi_ip_fields_are_translated(self):
        translated = translate_log_line(
            "I (10539) esp_netif_handlers: sta ip: 192.168.31.22, mask: 255.255.255.0, gw: 192.168.31.1"
        )
        self.assertIn("网络接口： 终端 IP： 192.168.31.22", translated)
        self.assertIn("子网掩码： 255.255.255.0", translated)
        self.assertIn("网关： 192.168.31.1", translated)

    def test_phase_and_cumulative_timing_are_parsed(self):
        line = "I (1234) 云端: 延时 DeepSeek=1800 ms，总计=23000 ms"
        match = _LATENCY_RE.search(line)
        self.assertIsNotNone(match)
        samples = {
            "录音结束": (4500, 4500),
            "有道识别": (3000, 8000),
            match.group("phase"): (int(match.group("phase_ms")), int(match.group("total_ms"))),
        }
        summary = format_latency_summary(samples)
        self.assertIn("AI 回复 1800 ms（唤醒后累计 23000 ms）", summary)

    def test_recording_end_timing_is_parsed(self):
        match = _UTTERANCE_LATENCY_RE.search("延时 录音结束：音频=3200 ms，唤醒后=5100 ms")
        self.assertIsNotNone(match)
        self.assertEqual(match.group("total_ms"), "5100")

    def test_asr_stages_update_gui_without_a_window(self):
        class Value:
            text = ""
            def set(self, text):
                self.text = text
        class View:
            latency_samples = {}
            latency_var = Value()
        view = View()
        for phase, duration in (("本地准备", 620), ("网络建连", 1140),
                                ("音频上传", 71000), ("等待响应", 200)):
            BridgeConfigurator.update_latency_from_log(
                view, f"[12:04:22] I (120589) 有道识别: 识别分段 {phase}={duration} ms")
        self.assertIn("音频上传 71000 ms", view.latency_var.text)
        self.assertIn("等待响应 200 ms", view.latency_var.text)
        self.assertIsNone(_ASR_PHASE_RE.search("识别请求计时：上传=123/123 字节"))


class OutputModeTests(unittest.TestCase):
    def test_local_tone_result_does_not_claim_acoustic_success(self):
        class Value:
            def set(self, text): self.text = text
        class View:
            footer_var = Value()
        view = View()
        BridgeConfigurator.handle_line(view, "@CFG BT_TONE_RESULT OK bytes=529200 elapsed=3000")
        self.assertIn("请确认音箱实际声音", view.footer_var.text)
        BridgeConfigurator.handle_line(view, "@CFG BT_TONE_RESULT FAIL bytes=1000 elapsed=10000")
        self.assertIn("消费过慢", view.footer_var.text)

    def test_mode_commands_target_s3_only(self):
        class Value:
            def __init__(self, text): self.text = text
            def get(self): return self.text
        class View:
            sent = []
            def send_s3(self, command): self.sent.append(command)
        view = View()
        for label in ("直连喇叭（仅 S3）", "蓝牙音箱（WROOM）"):
            view.output_mode_var = Value(label)
            BridgeConfigurator.save_output_mode(view)
        self.assertEqual(view.sent, ["S3_OUTPUT_SET speaker", "S3_OUTPUT_SET bluetooth"])

    def test_status_restores_saved_output_mode(self):
        class Value:
            def set(self, text): self.text = text
        class View:
            output_mode_var = Value()
            s3_wifi_var = Value()
            footer_var = Value()
        view = View()
        BridgeConfigurator.handle_s3_line(view, '@S3 STATUS {"outputMode":"speaker","wifiConnected":true,"ssid":"test"}')
        self.assertEqual(view.output_mode_var.text, "直连喇叭（仅 S3）")
        self.assertEqual(view.s3_wifi_var.text, "已连接 test")

if __name__ == "__main__":
    unittest.main()
