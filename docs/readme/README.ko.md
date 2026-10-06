# CyberCat 음성 비서

ESP32 기반 음성 비서 참고 프로젝트입니다. S3는 음성과 클라우드를, WROOM은 Bluetooth 오디오를, C3는 ESP-NOW 및 적외선 제어를 담당합니다.

## 구성 요소

- `master_s3/`: 웨이크 워드, 음성 인식, DeepSeek 대화 및 TTS.
- `bridge_wroom/`: TF 오디오, PCM 및 Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, 적외선 학습 및 송신.

## 시작하기

먼저 [설정](../configuration.md), [배선](../wiring.md), [빌드 및 플래시](../build-and-flash.md)를 읽으세요. 상세 가이드는 현재 중국어 간체로 제공됩니다. 클라우드 자격 증명은 직접 준비하고 API 키, 로그, 녹음 파일을 커밋하지 마세요.

제어 센터는 20개 언어를 지원하며 아랍어는 오른쪽에서 왼쪽으로 배치됩니다. 오디오 브리지는 USB로 설정합니다. [USB 설정 가이드](../../bridge_wroom/USB配置说明.md)를 참고하세요.
