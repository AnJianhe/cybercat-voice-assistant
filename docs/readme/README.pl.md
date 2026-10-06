# Asystent głosowy CyberCat

Projekt referencyjny asystenta głosowego ESP32: S3 obsługuje mowę i chmurę, WROOM dźwięk Bluetooth, a C3 ESP-NOW i sterowanie podczerwienią.

## Komponenty

- `master_s3/`: słowo wybudzające, rozpoznawanie mowy, czat DeepSeek i TTS.
- `bridge_wroom/`: dźwięk TF, PCM i Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, nauka i wysyłanie poleceń podczerwieni.

## Pierwsze kroki

Najpierw przeczytaj [konfigurację](../configuration.md), [okablowanie](../wiring.md) oraz [budowanie i flashowanie](../build-and-flash.md). Szczegółowe instrukcje są obecnie dostępne tylko po chińsku uproszczonym. Używaj własnych danych dostępowych do chmury; nie publikuj kluczy API, logów ani nagrań.

Centrum sterowania obsługuje 20 języków, w tym arabski układ od prawej do lewej. Most audio konfiguruje się przez USB; zobacz [instrukcję USB](../../bridge_wroom/USB配置说明.md).
