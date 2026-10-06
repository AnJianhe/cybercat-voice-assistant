"""Probe Youdao TTS without printing credentials."""

from __future__ import annotations

import hashlib
import pathlib
import re
import time
import sys
import urllib.parse
import urllib.request
import uuid


ROOT = pathlib.Path(__file__).resolve().parents[1]
SECRETS = ROOT / "master_s3" / "main" / "youdao_secrets.h"
audio_format = sys.argv[1] if len(sys.argv) > 1 else "pcm"
if audio_format not in {"pcm", "wav", "mp3"}:
    raise SystemExit("format must be pcm, wav, or mp3")
voice_name = sys.argv[2] if len(sys.argv) > 2 else "weixiaomei"
OUTPUT = ROOT / "diagnostics" / f"youdao-tts-test-{voice_name}.{audio_format}"


def macro(text: str, name: str) -> str:
    match = re.search(rf'#define\s+{name}\s+"([^"]+)"', text)
    if not match:
        raise RuntimeError(f"Missing {name}")
    return match.group(1)


raw = SECRETS.read_text(encoding="utf-8")
app_id = macro(raw, "YOUDAO_APP_ID")
app_secret = macro(raw, "YOUDAO_APP_SECRET")
query = "你好，语音桥测试。"
salt = str(uuid.uuid4())
current_time = str(int(time.time()))
sign_input = query if len(query) <= 20 else query[:10] + str(len(query)) + query[-10:]
signature = hashlib.sha256(
    (app_id + sign_input + salt + current_time + app_secret).encode("utf-8")
).hexdigest()
body = urllib.parse.urlencode(
    {
        "q": query,
        "appKey": app_id,
        "salt": salt,
        "sign": signature,
        "signType": "v3",
        "curtime": current_time,
        "format": audio_format,
        "speed": "1",
        "volume": "1",
        "voiceName": voice_name,
    }
).encode("ascii")
request = urllib.request.Request(
    "https://openapi.youdao.com/ttsapi", data=body, method="POST"
)
with urllib.request.urlopen(request, timeout=30) as response:
    audio = response.read()
    content_type = response.headers.get("Content-Type", "")
    status = response.status

OUTPUT.write_bytes(audio)
print(f"HTTP={status} TYPE={content_type} LEN={len(audio)} HEAD={audio[:16].hex()}")
