# CyberCat-Sprachassistent

ESP32-Referenzprojekt für einen Sprachassistenten: Der S3 übernimmt Sprache und Cloud, der WROOM Bluetooth-Audio und der C3 ESP-NOW sowie Infrarotsteuerung.

## Komponenten

- `master_s3/`: Aktivierungswort, Spracherkennung, DeepSeek-Chat und TTS.
- `bridge_wroom/`: TF-Audio, PCM und Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, Infrarotlernen und -übertragung.

## Erste Schritte

Lesen Sie zuerst [Konfiguration](../configuration.md), [Verdrahtung](../wiring.md) und [Build und Flash](../build-and-flash.md). Diese ausführlichen Anleitungen sind derzeit nur auf vereinfachtem Chinesisch verfügbar. Verwenden Sie eigene Cloud-Zugangsdaten und veröffentlichen Sie niemals API-Schlüssel, Protokolle oder Aufnahmen.

Die Steuerzentrale unterstützt 20 Sprachen, einschließlich eines arabischen RTL-Layouts. Die Audiobrücke wird per USB konfiguriert; siehe die [USB-Konfigurationsanleitung](../../bridge_wroom/USB配置说明.md).
