#!/usr/bin/env python3
"""赛博猫猫音频桥 USB 图形配置工具。"""

from __future__ import annotations

import json
import queue
import re
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

try:
    from .cybercat_i18n import LANGUAGE_NAMES, LANGUAGE_TO_CODE, translate, is_rtl, state_text, mood_text
except ImportError:  # script entry point launched by the Windows .cmd file
    from cybercat_i18n import LANGUAGE_NAMES, LANGUAGE_TO_CODE, translate, is_rtl, state_text, mood_text

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:  # pragma: no cover - shown to desktop users
    raise SystemExit("缺少 pyserial。请使用项目根目录的‘启动音频桥配置器.cmd’启动。") from exc


BAUD_RATE = 115200
APP_TITLE = "赛博猫猫 · 双板 USB 管理器"

MOOD_TEXT = {
    "boot_off": "正在启动",
    "idle_blue": "待机中",
    "recognizing_yellow": "正在识别语音",
    "speech_to_text_cyan": "正在转文字",
    "ai_reply_purple": "AI 正在回复",
    "text_to_speech_green": "正在合成语音",
    "recognition_red": "没有识别到指令",
    "backend_error_blink": "网络或服务异常",
    "backend_red": "网络或服务异常",
    "backend_green": "网络或服务异常",
    "fixed_pink": "正在执行离线指令",
}

# ESP-IDF / ESP32-A2DP diagnostics are emitted by upstream libraries in English.
# Translate their human-readable fragments in the local control center while
# preserving the original timestamps, numeric event codes, and unknown text.
_LOG_TRANSLATIONS = (
    ("ESP-ROM:", "芯片启动 ROM："),
    ("Build:", "构建信息："),
    ("rst:", "复位原因："),
    ("boot:", "启动方式："),
    ("Saved PC:", "已保存程序计数器："),
    ("SPIWP:", "SPI 闪存写保护状态："),
    ("mode:", "模式："),
    ("clock div:", "时钟分频："),
    ("load:", "加载地址："),
    ("len:", "长度："),
    ("entry ", "入口地址："),
    ("compile time", "编译时间"),
    ("Multicore bootloader", "多核引导程序"),
    ("chip revision:", "芯片版本："),
    ("efuse block revision:", "熔丝块版本："),
    ("Boot SPI Speed", "启动 SPI 速度"),
    ("SPI Mode", "SPI 模式"),
    ("SPI Flash Size", "SPI 闪存容量"),
    ("End of partition table", "分区表结束"),
    ("Loaded app from partition at offset", "已从分区偏移加载程序："),
    ("Found 8MB PSRAM device", "检测到 8MB PSRAM"),
    ("PSRAM memory test OK", "PSRAM 内存测试通过"),
    ("RAM available for dynamic allocation:", "可供动态分配的 RAM："),
    ("Started on CPU", "已在 CPU 上启动"),
    ("Calling app_main()", "正在调用主程序"),
    ("Project name:", "项目名称："),
    ("App version:", "程序版本："),
    ("ESP-IDF:", "ESP-IDF 版本："),
    ("Application information:", "应用程序信息："),
    ("wifi driver task:", "Wi-Fi 驱动任务："),
    ("wifi firmware version:", "Wi-Fi 固件版本："),
    ("wifi certification version:", "Wi-Fi 认证版本："),
    ("config NVS flash:", "NVS 闪存配置："),
    ("Init data frame dynamic rx buffer num:", "数据帧动态接收缓冲区数量："),
    ("Init static rx mgmt buffer num:", "静态接收管理缓冲区数量："),
    ("Init management short buffer num:", "短管理缓冲区数量："),
    ("Init dynamic tx buffer num:", "动态发送缓冲区数量："),
    ("Init static tx FG buffer num:", "静态前台发送缓冲区数量："),
    ("Init static rx buffer size:", "静态接收缓冲区大小："),
    ("Init static rx buffer num:", "静态接收缓冲区数量："),
    ("Init dynamic rx buffer num:", "动态接收缓冲区数量："),
    ("WiFi IRAM OP enabled", "已启用 Wi-Fi 指令高速缓存"),
    ("WiFi RX IRAM OP enabled", "已启用 Wi-Fi 接收指令高速缓存"),
    ("sta ip:", "终端 IP："),
    ("mask:", "子网掩码："),
    ("gw:", "网关："),
    ("The storage free size is", "存储剩余空间："),
    ("The partition size is", "分区容量："),
    ("Successfully", "成功"),
    ("successfully", "成功"),
    ("AFE Version:", "音频前端版本："),
    ("Input PCM Config:", "输入 PCM 配置："),
    ("AFE Pipeline:", "音频前端处理流程："),
    ("Build fst from commands.", "正在根据指令构建语法树。"),
    ("active speech commands:", "条有效语音指令："),
    ("Device discovery failed, continue to discover...", "本轮蓝牙扫描失败，继续扫描……"),
    ("Device discovery stopped.", "蓝牙扫描已停止。"),
    ("Discovery started.", "开始扫描蓝牙设备。"),
    ("Cancel device discovery ...", "正在取消蓝牙扫描……"),
    ("Target device found", "已找到目标设备"),
    ("--Compatibility: Compatible", "--兼容性：兼容"),
    ("--Compatibility: Not compatible", "--兼容性：不兼容"),
    ("--Name:", "--名称："),
    ("a2dp connecting to peer:", "正在连接蓝牙音频设备："),
    ("a2dp disconnected", "蓝牙音频已断开"),
    ("a2dp connected", "蓝牙音频已连接"),
    ("a2dp media ready checking ...", "正在检查蓝牙音频播放状态……"),
    ("a2dp media ready, starting ...", "蓝牙音频已就绪，开始播放……"),
    ("TTS PCM streamed to bridge:", "语音合成 PCM 音频流已发送到音频桥："),
    ("TTS MP3 streamed to bridge:", "语音合成 MP3 音频流已发送到音频桥："),
    ("USB TTS test result:", "USB 语音合成测试结果："),
    ("PCM streamed", "PCM 音频流已发送"),
    ("MP3 streamed", "MP3 音频流已发送"),
    ("ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT", "蓝牙底层连接已完成"),
    ("ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT", "蓝牙底层连接已断开"),
    ("ESP_BT_GAP_MODE_CHG_EVT", "蓝牙模式已变更"),
    ("Device discovery", "蓝牙设备扫描"),
    ("state CONNECTING", "状态：正在连接"),
    ("state CONNECTED", "状态：已连接"),
    ("state UNCONNECTED", "状态：未连接"),
    ("state DISCOVERING", "状态：正在扫描"),
    ("continue to discover", "继续扫描"),
    ("connection refused", "连接被拒绝"),
    ("ERROR opening socket", "打开网络套接字失败"),
    ("start_ssl_client", "启动安全网络连接"),
    ("HTTPClient: error", "HTTP 请求错误"),
    ("STA enable failed", "启用 Wi-Fi 客户端模式失败"),
    ("wifi nvs cfg alloc out of memory", "Wi-Fi 配置内存不足"),
    ("wifi_init: Failed to deinit Wi-Fi driver", "Wi-Fi 驱动清理失败"),
    ("Failed to deinit Wi-Fi", "Wi-Fi 清理失败"),
    ("NO_AP_FOUND", "未找到目标 Wi-Fi"),
    ("ESP_FAIL", "操作失败"),
    ("ESP_OK", "操作成功"),
    ("ESP_ERR_NO_MEM", "内存不足"),
    ("Got IP", "已获取网络地址"),
    ("Device connected", "设备已连接"),
    ("Device disconnected", "设备已断开"),
    ("failed", "失败"),
    ("Failed", "失败"),
    ("successfully", "成功"),
    ("Successfully", "成功"),
    ("connecting", "正在连接"),
    ("connected", "已连接"),
    ("disconnected", "已断开"),
    ("error", "错误"),
    ("Error", "错误"),
)


def translate_log_line(line: str) -> str:
    """Translate known vendor-log wording without touching protocol payloads."""
    translated = line.replace("[BT_AV]", "[蓝牙音频]")
    for source, target in (
        ("esp_image:", "固件映像："), ("octal_psram:", "八线 PSRAM："),
        ("esp_psram:", "PSRAM："), ("cpu_start:", "CPU 启动："),
        ("app_init:", "应用初始化："), ("heap_init:", "堆内存初始化："),
        ("spi_flash:", "SPI 闪存："), ("sleep_gpio:", "睡眠 GPIO："),
        ("main_task:", "主任务："), ("wifi_init:", "Wi-Fi 初始化："),
        ("phy_init:", "无线物理层初始化："), ("esp_netif_handlers:", "网络接口："),
        ("MODEL_LOADER:", "模型加载器："), ("AFE_CONFIG:", "音频前端配置："),
        ("AFE:", "音频前端："),
    ):
        translated = translated.replace(source, target)
    translated = re.sub(r"BluetoothA2DPSource\.cpp", "蓝牙音频库", translated)
    translated = re.sub(r"bt_app_av_sm_hdlr\(\)", "蓝牙连接状态处理", translated)
    translated = re.sub(r"filter_inquiry_scan_result\(\)", "蓝牙扫描结果处理", translated)
    translated = re.sub(r"app_gap_callback\(\)", "蓝牙事件回调", translated)
    translated = re.sub(r"bt_app_av_state_connected_hdlr\(\)", "蓝牙连接完成处理", translated)
    for source, target in _LOG_TRANSLATIONS:
        translated = translated.replace(source, target)
    return translated


_LATENCY_RE = re.compile(
    r"延时\s*(?P<phase>有道识别|DeepSeek|TTS 到音频桥|TTS 到喇叭)="
    r"(?P<phase_ms>\d+) ms(?:，总计=(?P<total_ms>\d+) ms)?"
)
_UTTERANCE_LATENCY_RE = re.compile(
    r"延时\s*录音结束：音频=(?P<audio_ms>\d+) ms，唤醒后=(?P<total_ms>\d+) ms"
)
_ASR_PHASE_RE = re.compile(
    r"识别分段\s*(?P<phase>本地准备|网络建连|音频上传|等待响应|读取结果|解析结果)="
    r"(?P<phase_ms>\d+) ms"
)


def format_latency_summary(samples: dict[str, tuple[int, int | None]]) -> str:
    labels = (("录音结束", "收音结束"), ("有道识别", "语音识别"),
              ("DeepSeek", "AI 回复"), ("TTS 到音频桥", "TTS 输出完成"), ("TTS 到喇叭", "S3 喇叭输出完成"))
    parts = []
    for key, label in labels:
        if key not in samples:
            continue
        phase_ms, total_ms = samples[key]
        text = f"{label} {phase_ms} ms"
        if total_ms is not None:
            text += f"（唤醒后累计 {total_ms} ms）"
        parts.append(text)
    if "TTS 到音频桥" in samples and samples["TTS 到音频桥"][1] is not None:
        parts.append(f"唤醒至 TTS 送达 {samples['TTS 到音频桥'][1]} ms")
    asr_parts = []
    for phase in ("本地准备", "网络建连", "音频上传", "等待响应", "读取结果", "解析结果"):
        key = f"识别:{phase}"
        if key in samples:
            asr_parts.append(f"{phase} {samples[key][0]} ms")
    if asr_parts:
        parts.append("识别明细：" + "、".join(asr_parts))
    return "本次耗时：" + ("；".join(parts) if parts else "等待计时日志")


def encode_hex(value: str) -> str:
    data = value.encode("utf-8")
    return data.hex() if data else "-"


def decode_hex(value: str) -> str:
    if value == "-":
        return ""
    return bytes.fromhex(value).decode("utf-8", errors="replace")


class BridgeConfigurator(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("960x820")
        self.minsize(760, 600)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

        self.locale_code = self._load_locale()
        self._localized_widgets: list[tuple[tk.Widget, str]] = []
        self._localized_tabs: list[tuple[ttk.Notebook, str, str]] = []
        self._tree_headings: list[tuple[ttk.Treeview, str, str]] = []
        self.output_mode_code = "bluetooth"
        self._bt_connected = False
        self._bt_name = ""
        self._tf_mounted = False
        self._s3_wifi_connected = False
        self._s3_wifi_ssid = ""
        self._mood_code = ""

        self.serial_port: serial.Serial | None = None
        self.s3_serial_port: serial.Serial | None = None
        self.reader_thread: threading.Thread | None = None
        self.s3_reader_thread: threading.Thread | None = None
        self.stop_reader = threading.Event()
        self.stop_s3_reader = threading.Event()
        self.rx_queue: queue.Queue[str] = queue.Queue()
        self.wifi_rows: dict[str, dict] = {}
        self.bt_rows: dict[str, dict] = {}

        self.port_var = tk.StringVar()
        self.connection_var = tk.StringVar(value="未连接")
        self.s3_connection_var = tk.StringVar(value="S3 未连接")
        self.bt_status_var = tk.StringVar(value="-")
        self.tf_status_var = tk.StringVar(value="-")
        self.mood_var = tk.StringVar(value="-")
        self.heap_var = tk.StringVar(value="-")
        self.s3_wifi_var = tk.StringVar(value="S3 未连接")
        self.tts_flow_var = tk.StringVar(value="待命：S3 请求 TTS → 按已保存模式输出声音")
        self.latency_var = tk.StringVar(value="本次耗时：等待 S3 计时日志")
        self.latency_samples: dict[str, tuple[int, int | None]] = {}
        self.footer_var = tk.StringVar(value="请插入音频桥数据线，然后点击连接")
        self.tts_test_running = False
        self.tts_flow_timeout_id: str | None = None

        self._build_styles()
        self._build_ui()
        self._capture_ui_texts(self)
        self._apply_language()
        self.refresh_ports()
        self.after(50, self.process_rx)
        self.after(3000, self.periodic_status)

    def _build_styles(self) -> None:
        style = ttk.Style(self)
        for theme in ("vista", "clam"):
            try:
                style.theme_use(theme)
                break
            except tk.TclError:
                pass
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 17, "bold"))
        style.configure("Status.TLabel", font=("Microsoft YaHei UI", 10))
        style.configure("Treeview", rowheight=27)

    def _build_ui(self) -> None:
        outer = ttk.Frame(self, padding=14)
        outer.pack(fill="both", expand=True)

        header = ttk.Frame(outer)
        header.pack(fill="x")
        ttk.Label(header, text="🐾 赛博猫猫控制中心", style="Title.TLabel").pack(side="left")
        ttk.Label(header, text="语言").pack(side="right", padx=(8, 4))
        self.language_var = tk.StringVar(value=LANGUAGE_NAMES.get(self.locale_code, LANGUAGE_NAMES["zh-CN"]))
        self.language_combo = ttk.Combobox(
            header, textvariable=self.language_var, state="readonly", width=17,
            values=tuple(LANGUAGE_NAMES.values()),
        )
        self.language_combo.pack(side="right", padx=(0, 12))
        self.language_combo.bind("<<ComboboxSelected>>", self._language_selected)
        ttk.Label(header, textvariable=self.connection_var, style="Status.TLabel").pack(side="right")

        connection = ttk.LabelFrame(outer, text="WROOM 音频桥数据线（蓝牙、TF 卡、播放）", padding=10)
        connection.pack(fill="x", pady=(12, 8))
        self.port_combo = ttk.Combobox(connection, textvariable=self.port_var, state="readonly", width=42)
        self.port_combo.pack(side="left", fill="x", expand=True)
        ttk.Button(connection, text="刷新端口", command=self.refresh_ports).pack(side="left", padx=7)
        self.connect_button = ttk.Button(connection, text="连接", command=self.toggle_connection)
        self.connect_button.pack(side="left")

        s3_connection = ttk.LabelFrame(outer, text="S3 数据线（Wi‑Fi 与对话日志）", padding=10)
        s3_connection.pack(fill="x", pady=(0, 8))
        self.s3_port_var = tk.StringVar()
        self.s3_port_combo = ttk.Combobox(s3_connection, textvariable=self.s3_port_var, state="readonly", width=42)
        self.s3_port_combo.pack(side="left", fill="x", expand=True)
        self.s3_connect_button = ttk.Button(s3_connection, text="连接 S3", command=self.toggle_s3_connection)
        self.s3_connect_button.pack(side="left", padx=(7, 0))
        ttk.Label(s3_connection, textvariable=self.s3_connection_var, style="Status.TLabel").pack(side="right")

        status = ttk.LabelFrame(outer, text="当前状态", padding=10)
        status.pack(fill="x", pady=8)
        items = (
            ("WROOM 蓝牙音响", self.bt_status_var),
            ("WROOM TF 卡", self.tf_status_var),
            ("WROOM 剩余内存", self.heap_var),
            ("S3 Wi‑Fi", self.s3_wifi_var),
            ("猫猫状态（S3）", self.mood_var),
        )
        for column, (name, variable) in enumerate(items):
            box = ttk.Frame(status)
            box.grid(row=0, column=column, sticky="nsew", padx=5)
            ttk.Label(box, text=name).pack(anchor="w")
            ttk.Label(box, textvariable=variable, font=("Microsoft YaHei UI", 10, "bold")).pack(anchor="w")
            status.columnconfigure(column, weight=1)

        notebook = ttk.Notebook(outer)
        self.notebook = notebook
        notebook.pack(fill="both", expand=True, pady=8)
        self._build_wifi_tab(notebook)
        self._build_bluetooth_tab(notebook)
        self._build_service_tab(notebook)
        self._build_log_tab(notebook)

        ttk.Label(outer, textvariable=self.footer_var, anchor="w").pack(fill="x", pady=(5, 0))

    @staticmethod
    def _load_locale() -> str:
        try:
            locale = (Path.home() / ".cybercat-ui-language").read_text(encoding="utf-8").strip()
            return locale if locale in LANGUAGE_NAMES else "zh-CN"
        except OSError:
            return "zh-CN"

    def _language_selected(self, _event=None) -> None:
        self.locale_code = LANGUAGE_TO_CODE.get(self.language_var.get(), "zh-CN")
        try:
            (Path.home() / ".cybercat-ui-language").write_text(self.locale_code, encoding="utf-8")
        except OSError:
            pass
        self._apply_language()

    def _capture_ui_texts(self, parent: tk.Misc) -> None:
        for widget in parent.winfo_children():
            try:
                source = str(widget.cget("text"))
            except (tk.TclError, AttributeError):
                source = ""
            if source and translate(source, "en") != source:
                self._localized_widgets.append((widget, source))
            self._capture_ui_texts(widget)

    def _apply_language(self) -> None:
        """Relabel static controls and mirror geometry for Arabic without touching logs/data."""
        locale = self.locale_code
        self.title(translate(APP_TITLE, locale))
        for widget, source in self._localized_widgets:
            try:
                widget.configure(text=translate(source, locale))
            except tk.TclError:
                pass
        if hasattr(self, "language_combo"):
            self.language_var.set(LANGUAGE_NAMES[locale])
            self.language_combo.configure(values=tuple(LANGUAGE_NAMES.values()))
        if hasattr(self, "notebook"):
            for notebook, tab_id, source in self._localized_tabs:
                notebook.tab(tab_id, text=translate(source, locale))
            rtl = is_rtl(locale)
            if not hasattr(self, "_notebook_tab_order"):
                self._notebook_tab_order = self.notebook.tabs()
            ordered_tabs = reversed(self._notebook_tab_order) if rtl else iter(self._notebook_tab_order)
            tabs = tuple(ordered_tabs)
            for index, tab_id in enumerate(tabs):
                self.notebook.insert(index, tab_id)
        for tree, column, source in self._tree_headings:
            tree.heading(column, text=translate(source, locale), anchor="e" if is_rtl(locale) else "w")
        if hasattr(self, "wifi_tree"):
            self.wifi_tree.configure(displaycolumns=("security", "rssi", "ssid") if is_rtl(locale) else ("ssid", "rssi", "security"))
        if hasattr(self, "bt_tree"):
            self.bt_tree.configure(displaycolumns=("selected", "rssi", "mac", "name") if is_rtl(locale) else ("name", "mac", "rssi", "selected"))
        if hasattr(self, "output_mode_combo"):
            self._refresh_output_mode_text()
        if hasattr(self, "connect_button"):
            self.connect_button.configure(text=translate("断开" if self.serial_port and self.serial_port.is_open else "连接", locale))
        if hasattr(self, "s3_connect_button"):
            self.s3_connect_button.configure(text=translate("断开 S3" if self.s3_serial_port and self.s3_serial_port.is_open else "连接 S3", locale))
        self._refresh_status_labels()
        self._mirror_layout(self, is_rtl(locale))

    def _refresh_status_labels(self) -> None:
        locale = self.locale_code
        connected = lambda yes: state_text("connected" if yes else "disconnected", locale)
        if hasattr(self, "connection_var"):
            port = self.port_var.get().split(" — ", 1)[0] if self.serial_port and self.serial_port.is_open else ""
            self.connection_var.set(f"{connected(bool(port))} {port}".strip())
        if hasattr(self, "s3_connection_var"):
            port = self.s3_port_var.get().split(" — ", 1)[0] if self.s3_serial_port and self.s3_serial_port.is_open else ""
            self.s3_connection_var.set(f"S3 · {connected(bool(port))} {port}".strip())
        if hasattr(self, "bt_status_var"):
            self.bt_status_var.set(f"{connected(self._bt_connected)} {self._bt_name}".strip() if self._bt_name else state_text("disconnected", locale))
        if hasattr(self, "tf_status_var"):
            self.tf_status_var.set(state_text("mounted" if self._tf_mounted else "not_detected", locale))
        if hasattr(self, "s3_wifi_var"):
            self.s3_wifi_var.set(f"{connected(self._s3_wifi_connected)} {self._s3_wifi_ssid}".strip() if self._s3_wifi_connected else state_text("disconnected", locale))
        if hasattr(self, "mood_var"):
            self.mood_var.set(mood_text(self._mood_code, locale) if self._mood_code else "-")

    @staticmethod
    def _mirror_compass(value: str) -> str:
        # Tk uses the word "center" as a special anchor, not a combination
        # of compass letters. Preserve it instead of turning it into an
        # invalid string such as "cwntwr".
        if value.lower() == "center":
            return value
        return value.translate(str.maketrans({"w": "e", "e": "w", "W": "E", "E": "W"}))

    def _mirror_layout(self, root: tk.Misc, rtl: bool) -> None:
        """Mirror pack/grid placement from saved original geometry; logs remain LTR."""
        def visit(parent: tk.Misc) -> None:
            try:
                children = parent.winfo_children()
            except (tk.TclError, AttributeError):
                return
            try:
                columns, _rows = parent.grid_size()
                column_count = max(1, columns)
            except (tk.TclError, AttributeError):
                column_count = 1
            for child in children:
                try:
                    manager = child.winfo_manager()
                    if manager == "pack":
                        info = getattr(child, "_cybercat_pack_info", None)
                        if info is None:
                            info = child.pack_info().copy()
                            child._cybercat_pack_info = info
                        info = info.copy()
                        side = info.get("side", "top")
                        if side in ("left", "right"):
                            info["side"] = ("right" if side == "left" else "left") if rtl else side
                        anchor = info.get("anchor")
                        if anchor:
                            info["anchor"] = self._mirror_compass(anchor) if rtl else anchor
                        child.pack_configure(**info)
                    elif manager == "grid":
                        info = getattr(child, "_cybercat_grid_info", None)
                        if info is None:
                            info = child.grid_info().copy()
                            child._cybercat_grid_info = info
                        info = info.copy()
                        span = int(info.get("columnspan", 1))
                        original_col = int(info.get("column", 0))
                        info["column"] = column_count - original_col - span if rtl else original_col
                        if info.get("sticky"):
                            info["sticky"] = self._mirror_compass(info["sticky"]) if rtl else info["sticky"]
                        child.grid_configure(**info)
                    if isinstance(child, (ttk.Label, ttk.Button, ttk.LabelFrame)):
                        child.configure(anchor="e" if rtl else "w")
                    if isinstance(child, (ttk.Entry, ttk.Combobox)):
                        child.configure(justify="right" if rtl else "left")
                except (tk.TclError, AttributeError, TypeError, ValueError):
                    pass
                visit(child)

        visit(root)

    def _refresh_output_mode_text(self) -> None:
        bluetooth = translate("蓝牙音箱（WROOM）", self.locale_code)
        speaker = translate("直连喇叭（仅 S3）", self.locale_code)
        self.output_mode_combo.configure(values=(bluetooth, speaker))
        self.output_mode_var.set(bluetooth if self.output_mode_code == "bluetooth" else speaker)

    def _output_mode_selected(self, _event=None) -> None:
        self.output_mode_code = "speaker" if self.output_mode_var.get() == translate("直连喇叭（仅 S3）", self.locale_code) else "bluetooth"

    def _register_tab(self, notebook: ttk.Notebook, widget: ttk.Frame, source: str) -> None:
        self._localized_tabs.append((notebook, str(widget), source))

    def _register_heading(self, tree: ttk.Treeview, column: str, source: str) -> None:
        self._tree_headings.append((tree, column, source))

    def _build_wifi_tab(self, notebook: ttk.Notebook) -> None:
        tab = ttk.Frame(notebook, padding=12)
        notebook.add(tab, text="S3 Wi‑Fi")
        self._register_tab(notebook, tab, "S3 Wi‑Fi")
        bar = ttk.Frame(tab)
        bar.pack(fill="x")
        ttk.Button(bar, text="扫描附近 Wi‑Fi", command=lambda: self.send_s3("S3_WIFI_SCAN")).pack(side="left")
        ttk.Label(bar, text="双击列表可填入网络名称").pack(side="left", padx=10)

        self.wifi_tree = ttk.Treeview(tab, columns=("ssid", "rssi", "security"), show="headings", height=9)
        self.wifi_tree.heading("ssid", text="网络名称")
        self.wifi_tree.heading("rssi", text="信号")
        self.wifi_tree.heading("security", text="安全")
        self._register_heading(self.wifi_tree, "ssid", "网络名称")
        self._register_heading(self.wifi_tree, "rssi", "信号")
        self._register_heading(self.wifi_tree, "security", "安全")
        self.wifi_tree.column("ssid", width=430)
        self.wifi_tree.column("rssi", width=90, anchor="center")
        self.wifi_tree.column("security", width=90, anchor="center")
        self.wifi_tree.pack(fill="both", expand=True, pady=10)
        self.wifi_tree.bind("<Double-1>", self.choose_wifi)

        form = ttk.Frame(tab)
        form.pack(fill="x")
        ttk.Label(form, text="网络名称").grid(row=0, column=0, sticky="w", pady=4)
        self.ssid_entry = ttk.Entry(form)
        self.ssid_entry.grid(row=0, column=1, sticky="ew", padx=8, pady=4)
        ttk.Label(form, text="密码").grid(row=1, column=0, sticky="w", pady=4)
        self.wifi_password_entry = ttk.Entry(form, show="●")
        self.wifi_password_entry.grid(row=1, column=1, sticky="ew", padx=8, pady=4)
        ttk.Button(form, text="保存并连接", command=self.save_wifi).grid(row=0, column=2, rowspan=2, padx=4)
        form.columnconfigure(1, weight=1)

    def _build_bluetooth_tab(self, notebook: ttk.Notebook) -> None:
        tab = ttk.Frame(notebook, padding=12)
        notebook.add(tab, text="蓝牙音响")
        self._register_tab(notebook, tab, "蓝牙音响")
        bar = ttk.Frame(tab)
        bar.pack(fill="x")
        ttk.Button(bar, text="刷新已发现设备", command=lambda: self.send("CFG BT_LIST")).pack(side="left")
        ttk.Button(bar, text="清除选择并重新扫描", command=self.rescan_bluetooth).pack(side="left", padx=7)
        ttk.Button(bar, text="重新连接已保存音响", command=lambda: self.send("CFG BT_RECONNECT")).pack(side="left")
        ttk.Button(tab, text="播放 3 秒本地测试音（仅 WROOM 蓝牙）",
                   command=lambda: self.send("CFG BT_TONE")).pack(anchor="w", pady=(8, 0))

        self.bt_tree = ttk.Treeview(tab, columns=("name", "mac", "rssi", "selected"), show="headings", height=12)
        for key, text, width in (
            ("name", "设备名称", 330),
            ("mac", "MAC 地址", 180),
            ("rssi", "信号", 80),
            ("selected", "已选择", 80),
        ):
            self.bt_tree.heading(key, text=text)
            self.bt_tree.column(key, width=width, anchor="center" if key != "name" else "w")
            self._register_heading(self.bt_tree, key, text)
        self.bt_tree.pack(fill="both", expand=True, pady=10)
        self.bt_tree.bind("<Double-1>", lambda _event: self.select_bluetooth())
        ttk.Button(tab, text="连接选中的蓝牙音响", command=self.select_bluetooth).pack(anchor="e")

    def _build_service_tab(self, notebook: ttk.Notebook) -> None:
        tab = ttk.Frame(notebook, padding=12)
        notebook.add(tab, text="服务与测试")
        self._register_tab(notebook, tab, "服务与测试")

        deepseek = ttk.LabelFrame(tab, text="DeepSeek API Key（发送到 S3，不回显）", padding=10)
        deepseek.pack(fill="x")
        self.deepseek_entry = ttk.Entry(deepseek, show="●")
        self.deepseek_entry.pack(side="left", fill="x", expand=True)
        ttk.Button(deepseek, text="更新 Key", command=self.save_deepseek).pack(side="left", padx=(8, 0))

        tts = ttk.LabelFrame(tab, text="有道语音合成测试", padding=10)
        tts.pack(fill="x", pady=12)
        self.tts_entry = ttk.Entry(tts)
        self.tts_entry.insert(0, "你好，我是赛博猫猫。")
        self.tts_entry.pack(side="left", fill="x", expand=True)
        self.tts_test_button = ttk.Button(tts, text="测试 TTS 全流程", command=self.test_tts)
        self.tts_test_button.pack(side="left", padx=(8, 0))
        flow = ttk.Frame(tab)
        flow.pack(fill="x", pady=(0, 12))
        ttk.Label(flow, textvariable=self.tts_flow_var).pack(anchor="w")
        self.tts_progress = ttk.Progressbar(flow, orient="horizontal", mode="determinate", maximum=5, value=0)
        self.tts_progress.pack(fill="x", pady=(5, 0))
        ttk.Label(flow, textvariable=self.latency_var, wraplength=820).pack(anchor="w", pady=(5, 0))

        controls = ttk.LabelFrame(tab, text="设备", padding=10)
        controls.pack(fill="x")
        ttk.Button(controls, text="刷新状态…", command=lambda: self.select_target_action("refresh")).pack(side="left")
        ttk.Button(controls, text="重启设备…", command=lambda: self.select_target_action("restart")).pack(side="left", padx=8)
        output = ttk.LabelFrame(tab, text="S3 声音输出（保存后立即生效；播放期间不可切换）", padding=10)
        output.pack(fill="x", pady=8)
        output_controls = ttk.Frame(output)
        output_controls.pack(fill="x")
        self.output_mode_var = tk.StringVar(value="蓝牙音箱（WROOM）")
        self.output_mode_combo = ttk.Combobox(output_controls, textvariable=self.output_mode_var, state="readonly", width=28,
                                               values=("蓝牙音箱（WROOM）", "直连喇叭（仅 S3）"))
        self.output_mode_combo.pack(side="left")
        self.output_mode_combo.bind("<<ComboboxSelected>>", self._output_mode_selected)
        ttk.Button(output_controls, text="保存输出模式", command=self.save_output_mode).pack(side="left", padx=8)
        ttk.Label(output, text="S3 → I2S 功放：BCLK=15，LRCLK=16，DIN=7；裸喇叭不可接 GPIO",
                  wraplength=820).pack(anchor="w", pady=(6, 0))

    def save_output_mode(self) -> None:
        locale = getattr(self, "locale_code", "zh-CN")
        mode = "speaker" if self.output_mode_var.get() == translate("直连喇叭（仅 S3）", locale) else "bluetooth"
        self.output_mode_code = mode
        self.send_s3(f"S3_OUTPUT_SET {mode}")

    def _build_log_tab(self, notebook: ttk.Notebook) -> None:
        bridge_tab = ttk.Frame(notebook, padding=12)
        notebook.add(bridge_tab, text="WROOM 日志")
        self._register_tab(notebook, bridge_tab, "WROOM 日志")
        bar = ttk.Frame(bridge_tab)
        bar.pack(fill="x")
        ttk.Button(bar, text="读取板内日志", command=lambda: self.send("CFG LOG_DUMP")).pack(side="left")
        ttk.Button(bar, text="查看存储状态", command=lambda: self.send("CFG LOG_STATUS")).pack(side="left", padx=7)
        ttk.Button(bar, text="清除板内日志", command=self.clear_device_log).pack(side="left")
        ttk.Button(bar, text="保存日志", command=lambda: self.export_log("bridge")).pack(side="right")
        ttk.Button(bar, text="清空窗口", command=lambda: self.bridge_log_text.delete("1.0", "end")).pack(side="right", padx=7)
        self.bridge_log_text = tk.Text(bridge_tab, wrap="word", font=("Consolas", 9), background="#101820", foreground="#d8f1ff")
        self.bridge_log_text.pack(fill="both", expand=True, pady=(10, 0))

        s3_tab = ttk.Frame(notebook, padding=12)
        notebook.add(s3_tab, text="S3 日志")
        self._register_tab(notebook, s3_tab, "S3 日志")
        s3_bar = ttk.Frame(s3_tab)
        s3_bar.pack(fill="x")
        ttk.Button(s3_bar, text="读取 S3 日志", command=lambda: self.send_s3("LOG_DUMP")).pack(side="left")
        ttk.Button(s3_bar, text="保存日志", command=lambda: self.export_log("s3")).pack(side="right")
        ttk.Button(s3_bar, text="清空窗口", command=lambda: self.s3_log_text.delete("1.0", "end")).pack(side="right", padx=7)
        self.s3_log_text = tk.Text(s3_tab, wrap="word", font=("Consolas", 9), background="#101820", foreground="#d8f1ff")
        self.s3_log_text.pack(fill="both", expand=True, pady=(10, 0))

    def refresh_ports(self) -> None:
        devices = list(list_ports.comports())
        values = [f"{item.device} — {item.description}" for item in devices]
        self.port_combo["values"] = values
        self.s3_port_combo["values"] = values
        if not values:
            self.port_var.set("")
            self.footer_var.set("没有发现串口，请检查数据线和 CH340/CP210x 驱动")
            return
        current = self.port_var.get()
        if current not in values:
            preferred = next(
                (value for value in values if any(tag in value.upper() for tag in ("CH340", "CH341", "CP210", "USB-SERIAL", "USB SERIAL"))),
                values[0],
            )
            self.port_var.set(preferred)
        if self.s3_port_var.get() not in values:
            s3_preferred = next((value for value in values if "USB 串行设备" in value), values[0])
            self.s3_port_var.set(s3_preferred)
        self.footer_var.set(f"发现 {len(values)} 个串口")

    def toggle_connection(self) -> None:
        if self.serial_port and self.serial_port.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        selected = self.port_var.get()
        if not selected:
            messagebox.showwarning(APP_TITLE, "没有可用串口。")
            return
        port = selected.split(" — ", 1)[0]
        try:
            connection = serial.Serial(port=None, baudrate=BAUD_RATE, timeout=0.25, write_timeout=2, dsrdtr=False, rtscts=False)
            connection.dtr = False
            connection.rts = False
            connection.port = port
            connection.open()
        except serial.SerialException as exc:
            messagebox.showerror(APP_TITLE, f"无法打开 {port}：\n{exc}\n\n请关闭 Arduino 串口监视器后重试。")
            return
        self.serial_port = connection
        self.stop_reader.clear()
        self.reader_thread = threading.Thread(target=self.reader_loop, daemon=True)
        self.reader_thread.start()
        self._refresh_status_labels()
        self.connect_button.configure(text=translate("断开", self.locale_code))
        self.footer_var.set("正在等待音频桥启动……")
        self.after(1200, lambda: self.send("CFG HELLO", quiet=True))

    def disconnect(self) -> None:
        self.stop_reader.set()
        connection, self.serial_port = self.serial_port, None
        if connection:
            try:
                connection.close()
            except serial.SerialException:
                pass
        self._refresh_status_labels()
        self.connect_button.configure(text=translate("连接", self.locale_code))
        self.footer_var.set("USB 已断开")

    def toggle_s3_connection(self) -> None:
        if self.s3_serial_port and self.s3_serial_port.is_open:
            self.disconnect_s3()
        else:
            self.connect_s3()

    def connect_s3(self) -> None:
        selected = self.s3_port_var.get()
        if not selected:
            messagebox.showwarning(APP_TITLE, "没有可用的 S3 串口。")
            return
        port = selected.split(" — ", 1)[0]
        try:
            self.s3_serial_port = serial.Serial(port=None, baudrate=BAUD_RATE, timeout=0.25, write_timeout=2,
                                                dsrdtr=False, rtscts=False)
            self.s3_serial_port.dtr = False
            self.s3_serial_port.rts = False
            self.s3_serial_port.port = port
            self.s3_serial_port.open()
        except serial.SerialException as exc:
            messagebox.showerror(APP_TITLE, f"无法打开 S3 串口 {port}：\n{exc}")
            return
        self.stop_s3_reader.clear()
        self.s3_reader_thread = threading.Thread(target=self.s3_reader_loop, daemon=True)
        self.s3_reader_thread.start()
        self._refresh_status_labels()
        self.s3_connect_button.configure(text=translate("断开 S3", self.locale_code))
        self.s3_wifi_var.set(state_text("connecting", self.locale_code))

    def disconnect_s3(self) -> None:
        self.stop_s3_reader.set()
        connection, self.s3_serial_port = self.s3_serial_port, None
        if connection:
            try:
                connection.close()
            except serial.SerialException:
                pass
        self._s3_wifi_connected = False
        self._s3_wifi_ssid = ""
        self._refresh_status_labels()
        self.s3_connect_button.configure(text=translate("连接 S3", self.locale_code))

    def reader_loop(self) -> None:
        while not self.stop_reader.is_set():
            connection = self.serial_port
            if not connection or not connection.is_open:
                break
            try:
                raw = connection.readline()
            except (serial.SerialException, OSError) as exc:
                self.rx_queue.put(f"@LOCAL DISCONNECTED {exc}")
                break
            if raw:
                self.rx_queue.put(raw.decode("utf-8", errors="replace").rstrip("\r\n"))

    def s3_reader_loop(self) -> None:
        while not self.stop_s3_reader.is_set():
            connection = self.s3_serial_port
            if not connection or not connection.is_open:
                break
            try:
                raw = connection.readline()
            except (serial.SerialException, OSError) as exc:
                self.rx_queue.put(("s3", f"@LOCAL DISCONNECTED {exc}"))
                break
            if raw:
                self.rx_queue.put(("s3", raw.decode("utf-8", errors="replace").rstrip("\r\n")))

    def send(self, command: str, quiet: bool = False) -> bool:
        connection = self.serial_port
        if not connection or not connection.is_open:
            if not quiet:
                messagebox.showwarning(APP_TITLE, "请先连接音频桥串口。")
            return False
        try:
            connection.write((command + "\n").encode("ascii"))
            connection.flush()
            return True
        except (serial.SerialException, OSError) as exc:
            if not quiet:
                messagebox.showerror(APP_TITLE, f"发送失败：{exc}")
            return False

    def send_s3(self, command: str, quiet: bool = False) -> bool:
        connection = self.s3_serial_port
        if not connection or not connection.is_open:
            if not quiet:
                messagebox.showwarning(APP_TITLE, "请先连接 S3 串口。")
            return False
        try:
            connection.write((command + "\n").encode("ascii"))
            connection.flush()
            return True
        except (serial.SerialException, OSError) as exc:
            if not quiet:
                messagebox.showerror(APP_TITLE, f"S3 发送失败：{exc}")
            return False

    def process_rx(self) -> None:
        for _ in range(250):
            try:
                item = self.rx_queue.get_nowait()
            except queue.Empty:
                break
            if isinstance(item, tuple):
                self.handle_s3_line(item[1])
            else:
                self.handle_line(item)
        self.after(50, self.process_rx)

    def handle_line(self, line: str) -> None:
        if line.startswith("@LOCAL DISCONNECTED"):
            self.footer_var.set("串口连接中断，请重新连接")
            self.disconnect()
            return
        if line.startswith("@CFG STATUS "):
            try:
                state = json.loads(line[len("@CFG STATUS "):])
                self.update_status(state)
            except json.JSONDecodeError:
                self.append_log(line)
            return
        if line == "@CFG WIFI_BEGIN":
            self.wifi_rows.clear()
            self.wifi_tree.delete(*self.wifi_tree.get_children())
            self.footer_var.set("正在扫描 Wi‑Fi……")
            return
        if line.startswith("@CFG WIFI "):
            self.add_wifi(json.loads(line[len("@CFG WIFI "):]))
            return
        if line.startswith("@CFG WIFI_END "):
            self.footer_var.set(f"Wi‑Fi 扫描完成，共 {len(self.wifi_rows)} 个网络")
            return
        if line == "@CFG BT_BEGIN":
            self.bt_rows.clear()
            self.bt_tree.delete(*self.bt_tree.get_children())
            return
        if line.startswith("@CFG BT "):
            self.add_bluetooth(json.loads(line[len("@CFG BT "):]))
            return
        if line.startswith("@CFG BT_END "):
            self.footer_var.set(f"已发现 {len(self.bt_rows)} 个蓝牙音频设备")
            return
        if line.startswith("@CFG BT_TONE_RESULT "):
            self.footer_var.set("本地测试音数字输出完成；请确认音箱实际声音" if " OK " in line
                                else "本地测试音失败：蓝牙消费过慢或连接中断，请查看 WROOM 日志")
            return
        if line.startswith("@CFG OK "):
            operation = line[len("@CFG OK "):]
            self.footer_var.set(f"操作成功：{operation}")
            if "RESTARTING" in operation:
                self.after(3000, lambda: self.send("CFG HELLO", quiet=True))
            return
        if line.startswith("@CFG ERROR "):
            self.finish_tts_flow("失败：" + line[len("@CFG ERROR "):], error=True)
            self.footer_var.set("操作失败：" + line[len("@CFG ERROR "):])
            messagebox.showerror(APP_TITLE, self.footer_var.get())
            return
        if line.startswith("@CFG HELLO ") or line.startswith("@CFG READY "):
            self.footer_var.set("音频桥 USB 配置通道正常")
            if line.startswith("@CFG READY "):
                self.send("CFG STATUS", quiet=True)
            return
        self.update_tts_flow_from_log(line)
        self.append_log(line)

    def handle_s3_line(self, line: str) -> None:
        if line.startswith("@LOCAL DISCONNECTED"):
            self.disconnect_s3()
            return
        if line.startswith("@S3 STATUS "):
            try:
                state = json.loads(line[len("@S3 STATUS "):])
                self.output_mode_code = "speaker" if state.get("outputMode") == "speaker" else "bluetooth"
                ssid = state.get("ssid") or "未配置"
                wifi_connected = bool(state.get("wifiConnected"))
                if hasattr(self, "_refresh_output_mode_text"):
                    self._s3_wifi_connected = wifi_connected
                    self._s3_wifi_ssid = ssid if wifi_connected else ""
                    self._refresh_output_mode_text()
                    self._refresh_status_labels()
                else:  # lightweight protocol test doubles
                    self.output_mode_var.set("直连喇叭（仅 S3）" if self.output_mode_code == "speaker" else "蓝牙音箱（WROOM）")
                    self.s3_wifi_var.set(("已连接 " if wifi_connected else "未连接 ") + ssid)
                self.footer_var.set(f"S3 状态已更新；剩余内存 {int(state.get('freeHeap', 0)) // 1024} KB")
                return
            except json.JSONDecodeError:
                pass
        if line.startswith("@S3 OUTPUT_SET "):
            result = line[len("@S3 OUTPUT_SET "):]
            self.footer_var.set("声音输出模式已保存" if result == "OK" else "输出模式未保存：" + result)
            self.send_s3("S3_STATUS", quiet=True)
            return
        if line == "@S3 WIFI_BEGIN":
            self.wifi_rows.clear()
            self.wifi_tree.delete(*self.wifi_tree.get_children())
            self.footer_var.set("S3 正在扫描 Wi‑Fi……")
            return
        if line.startswith("@S3 WIFI "):
            try:
                ssid_hex, rssi, secure = line[len("@S3 WIFI "):].split(" ", 2)
                self.add_wifi({"ssid": decode_hex(ssid_hex), "rssi": int(rssi), "secure": secure != "0"})
            except (ValueError, TypeError):
                self.append_log(line, "s3")
            return
        if line.startswith("@S3 WIFI_END "):
            self.footer_var.set(f"S3 Wi‑Fi 扫描完成，共 {len(self.wifi_rows)} 个网络")
            return
        if line.startswith("@S3 WIFI_ERROR "):
            self.footer_var.set("S3 Wi‑Fi 扫描失败：" + line[len("@S3 WIFI_ERROR "):])
            return
        if line == "@S3 WIFI_SET OK":
            self.footer_var.set("S3 Wi‑Fi 已保存，正在重新连接")
            self.after(3000, lambda: self.send_s3("S3_STATUS", quiet=True))
            return
        if line == "@S3 TTS_TEST QUEUED":
            self.tts_progress.configure(value=1)
            self.tts_flow_var.set("1/5 S3 已接收测试任务，正在请求有道 TTS……")
            self.footer_var.set("TTS 测试由 S3 执行，按已保存的输出模式播放")
            return
        self.update_latency_from_log(line)
        if "已唤醒，正在聆听" in line:
            self.latency_samples.clear()
            self.latency_var.set("本次耗时：已唤醒，等待本轮结束与云端计时")
        lower = line.lower()
        if "got ip" in lower or "wifi connected" in lower or "ip_event_sta_got_ip" in lower:
            self._s3_wifi_connected = True
            self._refresh_status_labels()
        elif "disconnected" in lower or "no ap found" in lower:
            self._s3_wifi_connected = False
            self._s3_wifi_ssid = ""
            self._refresh_status_labels()
        self.update_tts_flow_from_log(line)
        self.append_log(line, "s3")

    def update_status(self, state: dict) -> None:
        bt_name = state.get("btName") or "未选择"
        self._bt_connected = bool(state.get("btConnected"))
        self._bt_name = "" if bt_name == "未选择" else str(bt_name)
        self._tf_mounted = bool(state.get("tfMounted"))
        mood = state.get("s3Mood") or ""
        self._mood_code = str(mood)
        self.heap_var.set(f"{int(state.get('freeHeap', 0)) // 1024} KB")
        self._refresh_status_labels()
        self.footer_var.set("状态已更新")

    def add_wifi(self, network: dict) -> None:
        ssid = str(network.get("ssid", ""))
        if not ssid:
            return
        current = self.wifi_rows.get(ssid)
        if current and int(current.get("rssi", -999)) >= int(network.get("rssi", -999)):
            return
        if current:
            self.wifi_tree.delete(current["item"])
        item = self.wifi_tree.insert("", "end", values=(ssid, network.get("rssi", ""), "加密" if network.get("secure") else "开放"))
        network["item"] = item
        self.wifi_rows[ssid] = network

    def add_bluetooth(self, device: dict) -> None:
        mac = str(device.get("mac", ""))
        if not mac:
            return
        item = self.bt_tree.insert(
            "", "end",
            values=(device.get("name") or "未命名设备", mac, device.get("rssi", ""), "是" if device.get("selected") else ""),
        )
        device["item"] = item
        self.bt_rows[mac] = device

    def choose_wifi(self, _event=None) -> None:
        selection = self.wifi_tree.selection()
        if not selection:
            return
        ssid = str(self.wifi_tree.item(selection[0], "values")[0])
        self.ssid_entry.delete(0, "end")
        self.ssid_entry.insert(0, ssid)
        self.wifi_password_entry.focus_set()

    def save_wifi(self) -> None:
        ssid = self.ssid_entry.get()
        password = self.wifi_password_entry.get()
        if not ssid or len(ssid.encode("utf-8")) > 32 or len(password.encode("utf-8")) > 64:
            messagebox.showwarning(APP_TITLE, "Wi‑Fi 名称或密码长度无效。")
            return
        if self.send_s3(f"S3_WIFI_SET {encode_hex(ssid)} {encode_hex(password)}"):
            self.wifi_password_entry.delete(0, "end")

    def select_bluetooth(self) -> None:
        selection = self.bt_tree.selection()
        if not selection:
            messagebox.showwarning(APP_TITLE, "请先选择一个蓝牙音响。")
            return
        name, mac, *_ = self.bt_tree.item(selection[0], "values")
        if messagebox.askyesno(APP_TITLE, f"连接蓝牙音响“{name}”吗？\n设备会自动重启。"):
            self.send(f"CFG BT_SELECT {encode_hex(str(name))} {mac}")

    def rescan_bluetooth(self) -> None:
        if messagebox.askyesno(APP_TITLE, "清除当前蓝牙音响并重启扫描吗？"):
            self.send("CFG BT_RESCAN")

    def save_deepseek(self) -> None:
        key = self.deepseek_entry.get().strip()
        if len(key) < 16 or len(key.encode("utf-8")) > 127:
            messagebox.showwarning(APP_TITLE, "API Key 长度无效。")
            return
        if self.send(f"CFG DEEPSEEK_SET {encode_hex(key)}"):
            self.deepseek_entry.delete(0, "end")

    def test_tts(self) -> None:
        text = self.tts_entry.get().strip()
        if not text:
            messagebox.showwarning(APP_TITLE, "请输入测试文字。")
            return
        if self.tts_test_running:
            self.footer_var.set("TTS 流程正在执行，请等待播放结束")
            return
        if self.send_s3(f"S3_TTS_TEST {encode_hex(text)}"):
            self.latency_samples.clear()
            self.latency_var.set("本次耗时：TTS 测试已开始，等待计时日志")
            self.tts_test_running = True
            self.tts_test_button.configure(state="disabled")
            self.tts_progress.configure(value=0)
            self.tts_flow_var.set("开始：S3 请求有道 TTS，不切断蓝牙、不启动 WROOM Wi‑Fi")
            self.footer_var.set("TTS 全流程已启动")
            if self.tts_flow_timeout_id:
                self.after_cancel(self.tts_flow_timeout_id)
            self.tts_flow_timeout_id = self.after(90000, lambda: self.finish_tts_flow("超时：请查看日志", error=True))

    def update_tts_flow_from_log(self, line: str) -> None:
        if not self.tts_test_running:
            return
        if "USB TTS 测试结果：成功" in line:
            self.finish_tts_flow("完成：S3 音频输出结束（实物声音以听到为准）")
            return
        if "蓝牙音频已下载到 PSRAM" in line:
            self.tts_progress.configure(value=3)
            self.tts_flow_var.set("3/5 S3 音频下载完成，正在送给 WROOM 蓝牙播放")
            return
        if "USB TTS 测试结果：失败" in line or "ERR:PCM" in line:
            self.finish_tts_flow("失败：请查看对应设备日志", error=True)
            return
        if "语音合成首段 PCM" in line:
            self.tts_progress.configure(value=3)
            self.tts_flow_var.set("3/5 已收到音频，正在按输出模式连续播放……")
            return
        if "PCM 已送往S3 喇叭" in line:
            self.tts_progress.configure(value=5)
            self.tts_flow_var.set("5/5 S3 直连喇叭数字输出已结束")
            return
        if "TTS 开始" in line or "断开蓝牙" in line:
            self.tts_progress.configure(value=1)
            self.tts_flow_var.set("1/5 蓝牙已断开，正在释放内存……")
        elif "Wi-Fi 已连接" in line or "WiFi 已连接" in line or "连接 Wi-Fi" in line:
            self.tts_progress.configure(value=2)
            self.tts_flow_var.set("2/5 Wi‑Fi 已连接，正在请求有道 TTS……")
        elif "TTS 已缓存" in line or ("缓存" in line and "TTS" in line):
            self.tts_progress.configure(value=4)
            self.tts_flow_var.set("4/5 音频已保存，正在重新连接蓝牙……")
        elif "CACHED_PLAYING" in line or "开始播放" in line:
            self.tts_progress.configure(value=5)
            self.tts_flow_var.set("5/5 蓝牙已连接，正在播放缓存音频……")
        elif "TTS 播放结束" in line or "TTS_ENDED" in line:
            self.finish_tts_flow("完成：蓝牙已恢复，TTS 播放结束")
        elif "有道 TTS" in line or "TTS_RESULT API_OK" in line:
            self.tts_progress.configure(value=3)
            self.tts_flow_var.set("3/5 正在请求有道 TTS 并下载音频……")
        elif "OK:MP3_STARTED" in line:
            self.tts_progress.configure(value=3)
            self.tts_flow_var.set("3/5 WROOM 已创建 TF 缓存，正在可靠传输音频……")
        elif "TTS MP3 streamed" in line:
            self.tts_progress.configure(value=4)
            self.tts_flow_var.set("4/5 MP3 已完整传输，正在开始播放……")
        elif "TTS PCM streamed" in line or "PCM 已送往音频桥" in line:
            self.tts_progress.configure(value=4)
            self.tts_flow_var.set("4/5 PCM 音频流已送往音频桥")
        elif "OK:MP3_ENDED" in line:
            self.tts_progress.configure(value=5)
            self.tts_flow_var.set("5/5 WROOM 已收到完整 MP3，蓝牙正在播放……")
        elif "TTS 失败" in line or "ERR:TTS" in line or "BT_NOT_CONNECTED" in line:
            self.finish_tts_flow("失败：请查看日志详情", error=True)

    def update_latency_from_log(self, line: str) -> None:
        asr = _ASR_PHASE_RE.search(line)
        if asr:
            self.latency_samples[f"识别:{asr.group('phase')}"] = (int(asr.group("phase_ms")), None)
        utterance = _UTTERANCE_LATENCY_RE.search(line)
        if utterance:
            self.latency_samples["录音结束"] = (
                int(utterance.group("audio_ms")), int(utterance.group("total_ms")))
        match = _LATENCY_RE.search(line)
        if match:
            total = match.group("total_ms")
            self.latency_samples[match.group("phase")] = (
                int(match.group("phase_ms")), int(total) if total is not None else None)
        if utterance or match or asr:
            self.latency_var.set(format_latency_summary(self.latency_samples))

    def finish_tts_flow(self, message: str, error: bool = False) -> None:
        if self.tts_flow_timeout_id:
            self.after_cancel(self.tts_flow_timeout_id)
            self.tts_flow_timeout_id = None
        self.tts_test_running = False
        self.tts_test_button.configure(state="normal")
        if error:
            self.tts_flow_var.set(message)
        else:
            self.tts_progress.configure(value=5)
            self.tts_flow_var.set(message)

    def select_target_action(self, action: str) -> None:
        dialog = tk.Toplevel(self)
        dialog.title("选择操作对象")
        dialog.transient(self)
        dialog.grab_set()
        dialog.resizable(False, False)
        verb = "刷新状态" if action == "refresh" else "重启"
        ttk.Label(dialog, text=f"请选择要{verb}的设备：", font=("Microsoft YaHei UI", 11, "bold")).pack(padx=24, pady=(20, 12))
        choices = ttk.Frame(dialog)
        choices.pack(padx=20, pady=(0, 12))
        for target, label in (("bridge", "WROOM 音频桥"), ("s3", "S3 主控"), ("all", "全部")):
            ttk.Button(choices, text=label, width=16,
                       command=lambda t=target: self.run_target_action(action, t, dialog)).pack(side="left", padx=5)
        ttk.Button(dialog, text="取消", command=dialog.destroy).pack(pady=(0, 18))
        dialog.update_idletasks()
        self._mirror_layout(dialog, is_rtl(self.locale_code))
        dialog.geometry(f"+{self.winfo_rootx() + 180}+{self.winfo_rooty() + 180}")

    def run_target_action(self, action: str, target: str, dialog: tk.Toplevel) -> None:
        dialog.destroy()
        targets = ("bridge", "s3") if target == "all" else (target,)
        if action == "refresh":
            if "bridge" in targets:
                self.send("CFG STATUS", quiet=True)
            if "s3" in targets:
                self.send_s3("S3_STATUS", quiet=True)
            self.footer_var.set("已发送状态刷新命令")
            return
        label = {"bridge": "WROOM 音频桥", "s3": "S3 主控", "all": "两个设备"}[target]
        if not messagebox.askyesno(APP_TITLE, f"确定重启{label}吗？"):
            return
        if "bridge" in targets:
            self.send("CFG RESTART", quiet=True)
        if "s3" in targets:
            self.send_s3("S3_RESTART", quiet=True)
        self.footer_var.set(f"已发送{label}重启命令")

    def clear_device_log(self) -> None:
        if messagebox.askyesno(APP_TITLE, "确定清除音频桥内部保存的日志吗？"):
            self.send("CFG LOG_CLEAR")

    def append_log(self, line: str, source: str = "bridge") -> None:
        widget = self.s3_log_text if source == "s3" else self.bridge_log_text
        stamp = time.strftime("%H:%M:%S")
        widget.insert("end", f"[{stamp}] {translate_log_line(line)}\n")
        widget.see("end")
        lines = int(widget.index("end-1c").split(".")[0])
        if lines > 6000:
            widget.delete("1.0", "1000.0")

    def export_log(self, source: str) -> None:
        widget = self.s3_log_text if source == "s3" else self.bridge_log_text
        board = "S3" if source == "s3" else "WROOM"
        default = f"CyberCat_{board}_{time.strftime('%Y%m%d_%H%M%S')}.log"
        path = filedialog.asksaveasfilename(defaultextension=".log", initialfile=default, filetypes=(("日志文件", "*.log"), ("所有文件", "*.*")))
        if path:
            Path(path).write_text(widget.get("1.0", "end-1c"), encoding="utf-8")
            self.footer_var.set(f"日志已保存到 {path}")

    def periodic_status(self) -> None:
        self.send("CFG STATUS", quiet=True)
        self.send_s3("S3_STATUS", quiet=True)
        self.after(3000, self.periodic_status)

    def on_close(self) -> None:
        self.disconnect()
        self.disconnect_s3()
        self.destroy()


if __name__ == "__main__":
    BridgeConfigurator().mainloop()
