# 賽博貓貓語音助理

以 ESP32 為基礎的語音助理參考專案：S3 負責語音與雲端，WROOM 負責藍牙音訊，C3 負責 ESP-NOW 與紅外線控制。

## 模組

- `master_s3/`：喚醒、語音辨識、DeepSeek 對話與 TTS。
- `bridge_wroom/`：TF 音訊、PCM 與 Bluetooth Classic A2DP。
- `slave_c3/`：ESP-NOW、紅外線學習與發射。

## 開始使用

請先閱讀 [設定](../configuration.md)、[接線](../wiring.md) 和 [建置與燒錄](../build-and-flash.md)。這些詳細指南目前以簡體中文編寫。請使用自己的雲端服務憑證，切勿提交金鑰、日誌或錄音。

控制中心支援 20 種語言，包含阿拉伯語 RTL 版面。音訊橋透過 USB 設定；操作方式見 [USB 設定說明](../../bridge_wroom/USB配置說明.md)。
