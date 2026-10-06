# 依赖库清单

| 模块 | 库 | 版本 | 用途 | 来源 |
|---|---|---:|---|---|
| S3 | ESP-IDF | 5.4.x（建议 5.4.2） | WiFi、HTTPS、UART、I2S、ESP-NOW、FreeRTOS、NVS | [Espressif/esp-idf](https://github.com/espressif/esp-idf) |
| S3 | Espressif ESP-SR | 2.5.1（工程已锁定） | AFE、VAD、WakeNet、MultiNet 和模型构建 | [Espressif Component Registry](https://components.espressif.com/components/espressif/esp-sr) |
| S3 | cJSON | 随 ESP-SR/IDF 解析到的版本 | DeepSeek SSE JSON、Azure STT JSON | [Espressif cJSON component](https://components.espressif.com/components/espressif/cjson) |
| WROOM | Arduino-ESP32 | 2.0.17（PlatformIO espressif32 6.10.0） | Arduino、SD/SPI、FreeRTOS、Classic Bluetooth 基础 | [Espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) |
| WROOM | ESP32-A2DP | 1.8.7 | Bluetooth Classic A2DP Source；与 Arduino-ESP32 2.0.17 实际编译兼容 | [pschatzmann/ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP) |
| WROOM | ESP8266Audio | 1.9.9 | `AudioGeneratorMP3` 软件 MP3 解码；与 Arduino-ESP32 2.0.17 实际编译兼容 | [earlephilhower/ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) |
| C3 | Arduino-ESP32 | 2.0.17（PlatformIO espressif32 6.10.0） | WiFi STA、ESP-NOW、Preferences/NVS | [Espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) |
| C3 | Arduino-IRremote | 4.7.1 | GPIO6 红外发射、GPIO7 红外接收、raw 学习/重放 | [Arduino-IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote) |
| 云 ASR/TTS | Azure AI Speech REST | 服务 API | 16 kHz mono WAV → 文本；文本 → raw 16 kHz/16-bit/mono PCM | [Microsoft Speech REST](https://learn.microsoft.com/azure/ai-services/speech-service/rest-speech-to-text-short) |
| 云 LLM | DeepSeek Chat Completions | 当前公开 API | SSE 流式 Text → Text | [DeepSeek API](https://api-docs.deepseek.com/api/create-chat-completion) |

## 兼容性说明

- WROOM 必须是原版 ESP32（带 Bluetooth Classic），不能用 ESP32-S3/C3 代替 A2DP Source。
- 两个 Arduino 工程故意锁定 PlatformIO `espressif32@6.10.0`，避免 Arduino Core 3.x/ESP-IDF 5.x 与旧版 Classic BT 接口混搭。
- ESP-SR 模型不是普通源码。改变 ESP-SR 版本或模型选择后必须重新生成并烧录 `srmodels.bin`。
