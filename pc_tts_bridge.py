"""Play ESP32-S3 assistant replies through the Windows default audio device."""

from __future__ import annotations

import argparse
import base64
import hashlib
import pathlib
import re
import tempfile
import time
import urllib.parse
import urllib.request
import uuid
import winsound

import serial
from serial.tools import list_ports


ROOT = pathlib.Path(__file__).resolve().parent
SECRETS = ROOT / "master_s3" / "main" / "youdao_secrets.h"
TTS_URL = "https://openapi.youdao.com/ttsapi"


def macro(text: str, name: str) -> str:
    match = re.search(rf'#define\s+{name}\s+"([^"]+)"', text)
    if not match:
        raise RuntimeError(f"缺少本地配置 {name}")
    return match.group(1)


def load_credentials() -> tuple[str, str]:
    raw = SECRETS.read_text(encoding="utf-8")
    return macro(raw, "YOUDAO_APP_ID"), macro(raw, "YOUDAO_APP_SECRET")


def available_ports() -> list:
    return list(list_ports.comports())


def choose_port(requested: str | None) -> str:
    ports = available_ports()
    if requested:
        if not any(p.device.casefold() == requested.casefold() for p in ports):
            raise RuntimeError(f"找不到串口 {requested}")
        return requested
    candidates = [
        p for p in ports
        if any(word in f"{p.description} {p.manufacturer}".casefold()
               for word in ("esp", "jtag", "usb serial", "ch34", "cp210"))
    ]
    if len(candidates) == 1:
        return candidates[0].device
    visible = ", ".join(f"{p.device} ({p.description})" for p in ports) or "无"
    raise RuntimeError(f"无法唯一确定 S3 串口；当前端口：{visible}。请使用 --port COM号")


def sign_input(text: str) -> str:
    return text if len(text) <= 20 else text[:10] + str(len(text)) + text[-10:]


def synthesize_wav(text: str, app_id: str, app_secret: str) -> bytes:
    salt = str(uuid.uuid4())
    current_time = str(int(time.time()))
    signature = hashlib.sha256(
        (app_id + sign_input(text) + salt + current_time + app_secret).encode("utf-8")
    ).hexdigest()
    body = urllib.parse.urlencode(
        {
            "q": text,
            "appKey": app_id,
            "salt": salt,
            "sign": signature,
            "signType": "v3",
            "curtime": current_time,
            "format": "wav",
            "speed": "1",
            "volume": "1",
            "voiceName": "youxiaoqin",
        }
    ).encode("ascii")
    request = urllib.request.Request(TTS_URL, data=body, method="POST")
    with urllib.request.urlopen(request, timeout=35) as response:
        audio = response.read()
        content_type = response.headers.get("Content-Type", "").casefold()
    if not audio.startswith(b"RIFF") or "audio" not in content_type:
        preview = audio[:200].decode("utf-8", errors="replace")
        raise RuntimeError(f"有道 TTS 返回的不是 WAV：{preview}")
    return audio


def send_status(port: serial.Serial, status: str) -> None:
    port.write((status + "\n").encode("ascii"))
    port.flush()


def play_reply(port: serial.Serial, text: str, app_id: str, app_secret: str) -> None:
    print(f"\nAI：{text}\n")
    send_status(port, "PC_TTS_STARTED")
    path: pathlib.Path | None = None
    try:
        audio = synthesize_wav(text, app_id, app_secret)
        with tempfile.NamedTemporaryFile(prefix="cyber-cat-", suffix=".wav", delete=False) as file:
            file.write(audio)
            path = pathlib.Path(file.name)
        winsound.PlaySound(str(path), winsound.SND_FILENAME)
        send_status(port, "PC_TTS_ENDED")
        print("播放完成，继续等待“小爱同学”……")
    except Exception:
        send_status(port, "PC_TTS_ERROR")
        raise
    finally:
        if path:
            path.unlink(missing_ok=True)


def run(port_name: str) -> None:
    app_id, app_secret = load_credentials()
    print(f"电脑音响桥启动：{port_name} @ 115200")
    print("声音将从 Windows 当前默认输出设备播放。按 Ctrl+C 停止。")
    chunks: list[str] | None = None
    expected_bytes = 0
    with serial.Serial(port_name, 115200, timeout=0.25, write_timeout=2) as port:
        while True:
            raw_line = port.readline()
            if not raw_line:
                continue
            line = raw_line.decode("utf-8", errors="replace").strip()
            if line.startswith("PC_TTS_TEXT_BEGIN:"):
                expected_bytes = int(line.rsplit(":", 1)[1])
                chunks = []
            elif line.startswith("PC_TTS_B64:") and chunks is not None:
                chunks.append(line.partition(":")[2])
            elif line == "PC_TTS_TEXT_END" and chunks is not None:
                payload = base64.b64decode("".join(chunks), validate=True)
                chunks = None
                if len(payload) != expected_bytes:
                    send_status(port, "PC_TTS_ERROR")
                    raise RuntimeError(
                        f"串口文字长度不匹配：期望 {expected_bytes}，收到 {len(payload)}"
                    )
                play_reply(port, payload.decode("utf-8"), app_id, app_secret)
            elif line:
                print(f"[S3] {line}")


def main() -> None:
    parser = argparse.ArgumentParser(description="赛博猫猫 S3 电脑音响桥")
    parser.add_argument("--port", help="S3 串口，例如 COM6；省略时自动识别")
    parser.add_argument("--list-ports", action="store_true", help="列出当前串口后退出")
    args = parser.parse_args()
    if args.list_ports:
        for item in available_ports():
            print(f"{item.device}\t{item.description}")
        return
    try:
        run(choose_port(args.port))
    except KeyboardInterrupt:
        print("\n电脑音响桥已停止。")


if __name__ == "__main__":
    main()
