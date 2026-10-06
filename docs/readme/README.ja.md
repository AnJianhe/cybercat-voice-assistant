# CyberCat 音声アシスタント

ESP32 ベースの音声アシスタント参考プロジェクトです。S3 が音声処理とクラウド、WROOM が Bluetooth オーディオ、C3 が ESP-NOW と赤外線制御を担当します。

## 構成

- `master_s3/`：ウェイクワード、音声認識、DeepSeek 対話、TTS。
- `bridge_wroom/`：TF 音声、PCM、Bluetooth Classic A2DP。
- `slave_c3/`：ESP-NOW、赤外線学習と送信。

## はじめに

まず[設定](../configuration.md)、[配線](../wiring.md)、[ビルドと書き込み](../build-and-flash.md)を確認してください。詳細ガイドは現在、簡体字中国語のみです。クラウド認証情報は自分で用意し、API キー、ログ、録音をコミットしないでください。

コントロールセンターは 20 言語に対応し、アラビア語では右から左のレイアウトを使用します。オーディオブリッジは USB で設定します。[USB 設定ガイド](../../bridge_wroom/USB配置说明.md)を参照してください。
