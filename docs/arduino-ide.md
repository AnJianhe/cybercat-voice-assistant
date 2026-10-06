# 使用 Arduino IDE

## 支持范围

- ESP32-WROOM-32 Audio Bridge：支持 Arduino IDE。
- ESP32-C3 Slave：支持 Arduino IDE。
- ESP32-S3 Master：不使用 Arduino IDE。它依赖 ESP-IDF 5.x、ESP-SR Component Manager、`menuconfig` 和模型分区，必须使用 ESP-IDF 工具链。

两个 Arduino 工程根目录已经加入与文件夹同名的 `.ino` 入口；实际实现仍位于 `src/`，Arduino IDE 会递归编译该目录。

## 1. 安装开发板支持

在 Arduino IDE 2.x 的“文件 → 首选项 → 其他开发板管理器网址”加入：

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

打开“开发板管理器”，安装：

```text
esp32 by Espressif Systems 2.0.17
```

不要随意换到 3.x；当前 A2DP/MP3 组合已经在 2.0.17 上实际编译通过。

## 2. WROOM Bridge

安装库：

- ESP32-A2DP 1.8.7：从 [v1.8.7 源码 ZIP](https://github.com/pschatzmann/ESP32-A2DP/archive/refs/tags/v1.8.7.zip) 下载，然后“项目 → 加载库 → 添加 .ZIP 库”。
- ESP8266Audio 1.9.9：在库管理器搜索 `ESP8266Audio`，选择 1.9.9；若管理器没有旧版本，可用 [1.9.9 ZIP](https://github.com/earlephilhower/ESP8266Audio/archive/refs/tags/1.9.9.zip)。

操作：

1. 用 Arduino IDE 打开 `bridge_wroom/bridge_wroom.ino`。
2. 修改 `bridge_wroom/src/config.h` 中的 `BLUETOOTH_SPEAKER_NAME`。
3. 开发板选择 `ESP32 Dev Module`。
4. Partition Scheme 选择 `Huge APP (3MB No OTA/1MB SPIFFS)`。
5. CPU Frequency 选择 240 MHz，Flash Frequency 选择 80 MHz。
6. 选择串口并上传，串口监视器设为 115200。

启动日志应出现 `Cyber Cat Bridge ready`。如果显示 `TF mount failed`，先检查卡格式和 GPIO5/18/19/23 接线。

## 3. ESP32-C3 Slave

安装库：

- 在库管理器搜索 `IRremote`，安装 4.7.1。

操作：

1. 用 Arduino IDE 打开 `slave_c3/slave_c3.ino`。
2. 修改 `slave_c3/src/config.h` 中的 `ESPNOW_WIFI_CHANNEL`。
3. 开发板选择 `ESP32C3 Dev Module`；若你的板明确是 DevKitM-1，也可选择对应条目。
4. USB CDC On Boot 按开发板串口方式选择；原生 USB 通常设为 Enabled，外置 USB-UART 通常不需要。
5. 选择串口并上传，串口监视器设为 115200。

启动日志会打印 `C3 STA MAC`。记录这个地址，写入 Master 的 `C3_MAC_0..5`。

## 常见问题

- `Multiple libraries were found`：删除重复的 A2DP、ESP8266Audio 或 IRremote 版本，只保留上述锁定版本。
- `Sketch too big`：WROOM 没有选择 Huge APP 分区。
- 找不到 `BluetoothA2DPSource.h`：ESP32-A2DP ZIP 未正确安装。
- 找不到 `AudioGeneratorMP3.h`：ESP8266Audio 未安装或版本目录损坏。
- C3 收不到 ESP-NOW：家庭 AP 信道与 `ESPNOW_WIFI_CHANNEL` 不一致。
- Arduino IDE 只适用于 WROOM/C3；S3 模型部署仍按 [build-and-flash.md](build-and-flash.md) 操作。

