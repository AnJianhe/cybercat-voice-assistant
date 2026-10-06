# Trợ lý giọng nói CyberCat

Dự án tham khảo trợ lý giọng nói ESP32: S3 xử lý giọng nói và đám mây, WROOM xử lý âm thanh Bluetooth, còn C3 cung cấp ESP-NOW và điều khiển hồng ngoại.

## Thành phần

- `master_s3/`: từ đánh thức, nhận dạng giọng nói, trò chuyện DeepSeek và TTS.
- `bridge_wroom/`: âm thanh TF, PCM và Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, học và phát lệnh hồng ngoại.

## Bắt đầu

Đọc [cấu hình](../configuration.md), [đấu dây](../wiring.md) và [biên dịch, nạp firmware](../build-and-flash.md) trước. Tài liệu chi tiết hiện chỉ có bằng tiếng Trung giản thể. Hãy dùng thông tin xác thực đám mây của riêng bạn; không công khai khóa API, nhật ký hoặc bản ghi âm.

Trung tâm điều khiển hỗ trợ 20 ngôn ngữ, gồm bố cục tiếng Ả Rập từ phải sang trái. Cấu hình cầu nối âm thanh qua USB; xem [hướng dẫn USB](../../bridge_wroom/USB配置说明.md).
