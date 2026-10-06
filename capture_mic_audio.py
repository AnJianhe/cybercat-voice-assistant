import argparse
import base64
from datetime import datetime
import pathlib
import re
import sys
import time
import wave

import serial


PORT = "COM4"
BAUD = 115200
OUTPUT_DIR = pathlib.Path(r"E:\系统默认\文档\ChatGPT\猫\diagnostics")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wait-for-enter", action="store_true",
                        help="Open the port, then wait for Enter before requesting a recording")
    args = parser.parse_args()
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    chunks = []
    expected_bytes = None
    receiving = False

    ser = serial.Serial()
    ser.port = PORT
    ser.baudrate = BAUD
    ser.timeout = 1
    ser.dtr = False
    ser.rts = False
    ser.open()
    print("serial_open", flush=True)

    try:
        if args.wait_for_enter:
            input("ready_press_enter_to_record\n")
        ser.reset_input_buffer()
        ser.write(b"MIC_RECORD\n")
        ser.flush()
        print("record_request_sent", flush=True)
        deadline = time.monotonic() + 240
        while time.monotonic() < deadline:
            raw_line = ser.readline()
            if not raw_line:
                continue
            line = raw_line.decode("ascii", errors="ignore").strip()
            if "MIC_CAPTURE_ERROR:" in line:
                raise RuntimeError(line)
            if "MIC_CAPTURE_ARMED" in line:
                print("capture_armed_3s", flush=True)
            if "MIC_CAPTURE_RECORDING" in line:
                print("recording_now", flush=True)
            if "MIC_CAPTURE_DATA_BEGIN:" in line:
                expected_bytes = int(line.split("MIC_CAPTURE_DATA_BEGIN:", 1)[1].split()[0])
                receiving = True
                print(f"receiving:{expected_bytes}", flush=True)
                continue
            if receiving and "MICB64:" in line:
                payload = line.split("MICB64:", 1)[1]
                payload = "".join(re.findall(r"[A-Za-z0-9+/=]", payload))
                if payload:
                    chunks.append(payload)
                continue
            if receiving and "MIC_CAPTURE_DATA_END" in line:
                break
        else:
            raise TimeoutError("timed out waiting for microphone capture")
    finally:
        ser.close()

    pcm = base64.b64decode("".join(chunks))
    if expected_bytes is None or len(pcm) != expected_bytes:
        raise RuntimeError(f"capture size mismatch: got {len(pcm)}, expected {expected_bytes}")

    capture_name = "s3_microphone_left_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    wav_path = OUTPUT_DIR / (capture_name + ".wav")
    with wave.open(str(wav_path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(16000)
        wav.writeframes(pcm)

    import lameenc

    encoder = lameenc.Encoder()
    encoder.set_bit_rate(64)
    encoder.set_in_sample_rate(16000)
    encoder.set_channels(1)
    encoder.set_quality(2)
    mp3 = encoder.encode(pcm) + encoder.flush()
    mp3_path = OUTPUT_DIR / (capture_name + ".mp3")
    mp3_path.write_bytes(mp3)
    print(f"wav:{wav_path}", flush=True)
    print(f"mp3:{mp3_path}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
