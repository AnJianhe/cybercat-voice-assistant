"""Youdao ASR auth check using generated silence, never a private recording."""
import argparse
import base64
import hashlib
import io
import json
import pathlib
import re
import time
import uuid
import wave

import requests


def truncate_audio(q):
    return q if len(q) <= 20 else q[:10] + str(len(q)) + q[-10:]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sign-type", choices=("v2", "v3"), default="v2")
    args = parser.parse_args()
    config = pathlib.Path(__file__).resolve().parents[1] / "master_s3/main/youdao_secrets.h"
    values = dict(re.findall(r'#define\s+(YOUDAO_APP_\w+)\s+"([^"]*)"', config.read_text(encoding="utf-8")))
    audio = io.BytesIO()
    with wave.open(audio, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16000)
        wav.writeframes(bytes(32000))
    q = base64.b64encode(audio.getvalue()).decode("ascii")
    salt, now = str(uuid.uuid4()), str(int(time.time()))
    signature = hashlib.sha256(
        (values["YOUDAO_APP_ID"] + truncate_audio(q) + salt + now +
         values["YOUDAO_APP_SECRET"]).encode("utf-8")
    ).hexdigest()
    fields = dict(q=q, appKey=values["YOUDAO_APP_ID"], salt=salt, curtime=now,
                  sign=signature, signType=args.sign_type, format="wav", rate="16000",
                  channel="1", type="1", langType="zh-CHS")
    try:
        response = requests.post("https://openapi.youdao.com/asrapi", data=fields, timeout=30)
        result = response.json()
    except (requests.RequestException, ValueError) as exc:
        print(json.dumps({"failure": type(exc).__name__}))
        return 1
    print(json.dumps({"http_status": response.status_code, "sign_type": args.sign_type,
                      "errorCode": result.get("errorCode"),
                      "result": result.get("result")}, ensure_ascii=False))
    # 4304 means empty recognition, expected for this generated-silence probe.
    return 0 if response.status_code == 200 and str(result.get("errorCode")) in ("0", "4304") else 1


if __name__ == "__main__":
    raise SystemExit(main())
