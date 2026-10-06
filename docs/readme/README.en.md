# CyberCat Voice Assistant

An ESP32 voice-assistant reference project: the S3 handles speech and cloud services, WROOM handles Bluetooth audio, and C3 provides ESP-NOW and infrared control.

## Components

- `master_s3/`: wake word, speech recognition, DeepSeek chat, and TTS.
- `bridge_wroom/`: TF audio, PCM, and Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, infrared learning, and transmission.

## Getting started

Read [configuration](../configuration.md), [wiring](../wiring.md), and [build and flash](../build-and-flash.md) first. These detailed guides are currently in Simplified Chinese. Use your own cloud credentials; never commit API keys, logs, or recordings.

The control center supports 20 languages, including an Arabic right-to-left layout. Configure the audio bridge over USB; see the [USB configuration guide](../../bridge_wroom/USB配置说明.md).
