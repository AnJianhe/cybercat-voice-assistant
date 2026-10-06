# 赛博猫猫（Cyber Cat）

三芯片全屋 AI 语音助手参考工程：

- `master_s3/`：ESP32-S3 N16R8，ESP-IDF 5.x + ESP-SR，负责唤醒、命令识别、云端 ASR、DeepSeek、TTS、UART、ESP-NOW 和闹钟。
- `bridge_wroom/`：ESP32-WROOM-32，Arduino，负责 TF/MP3、实时 PCM、重采样和 Bluetooth Classic A2DP Source。
- `slave_c3/`：ESP32-C3，Arduino，负责 ESP-NOW、红外学习、NVS 保存和发射。
- `docs/`：协议、接线、依赖、构建烧录和分模块验收说明。

## 真实性边界

这是可移植的参考工程，不假设所有 ESP32-S3 N16R8 开发板拥有相同麦克风。`master_s3/main/app_config.h` 中的 I2S 麦克风引脚必须按实际硬件确认。固定的 UART、TF 和 C3 红外引脚没有改动。

开放式语音采用 **Azure AI Speech REST**（16 kHz PCM WAV STT；16 kHz/16-bit/mono raw PCM TTS），DeepSeek 只执行 Text → Text。云服务均需要用户自己的有效凭据和联网环境。离线时 WakeNet、MultiNet、家电命令、本地 TF 音乐和已设置且设备未掉电的闹钟仍可运行；若要求断电后无网络仍保持准确墙上时间，必须额外安装 RTC（如 DS3231），ESP32 本身不能凭空恢复绝对时间。

## 首次阅读

1. 阅读 [构建与烧录](docs/build-and-flash.md)。
   如果你使用 Arduino IDE，先阅读 [Arduino IDE 指南](docs/arduino-ide.md)。
2. 按 [用户配置宏](docs/configuration.md) 填写三个工程的配置。
3. 按 [接线表](docs/wiring.md) 接线，S3 与 WROOM 必须共地。
4. 先单独验证 C3，再验证 WROOM，最后构建 S3 和 ESP-SR 模型。

UART 二进制协议见 [protocol.md](docs/protocol.md)，依赖版本见 [dependencies.md](docs/dependencies.md)。
实际编译结果与仍需上板验证的边界见 [validation.md](docs/validation.md)。

音频桥已关闭配置热点和网页服务器。需要修改 Wi‑Fi、选择蓝牙音响、更新 DeepSeek Key 或读取日志时，用 USB 数据线连接电脑并双击项目根目录的 `启动音频桥配置器.cmd`。详细说明见 [bridge_wroom/USB配置说明.md](bridge_wroom/USB配置说明.md)。

## 多语言 README

简体中文（本页） · [繁體中文](docs/readme/README.zh-TW.md) · [English](docs/readme/README.en.md) · [日本語](docs/readme/README.ja.md) · [한국어](docs/readme/README.ko.md) · [Français](docs/readme/README.fr.md) · [Deutsch](docs/readme/README.de.md) · [Español](docs/readme/README.es.md) · [Português](docs/readme/README.pt.md) · [Русский](docs/readme/README.ru.md) · [العربية](docs/readme/README.ar.md) · [हिन्दी](docs/readme/README.hi.md) · [Italiano](docs/readme/README.it.md) · [Nederlands](docs/readme/README.nl.md) · [Türkçe](docs/readme/README.tr.md) · [Tiếng Việt](docs/readme/README.vi.md) · [ไทย](docs/readme/README.th.md) · [Bahasa Indonesia](docs/readme/README.id.md) · [Bahasa Melayu](docs/readme/README.ms.md) · [Polski](docs/readme/README.pl.md)

控制中心也支持上述 20 种语言；阿拉伯语界面采用从右向左布局。设备日志内容仍按中文显示，避免改变协议和诊断原文。
