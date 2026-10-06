# Assistente vocale CyberCat

Progetto di riferimento per un assistente ESP32: S3 gestisce voce e cloud, WROOM l’audio Bluetooth e C3 ESP-NOW e il controllo a infrarossi.

## Componenti

- `master_s3/`: parola di attivazione, riconoscimento vocale, chat DeepSeek e TTS.
- `bridge_wroom/`: audio TF, PCM e Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, apprendimento e trasmissione a infrarossi.

## Per iniziare

Leggi [configurazione](../configuration.md), [cablaggio](../wiring.md) e [build e flash](../build-and-flash.md). Le guide dettagliate sono attualmente solo in cinese semplificato. Usa credenziali cloud personali e non pubblicare chiavi API, log o registrazioni.

Il centro di controllo supporta 20 lingue, compreso il layout arabo da destra a sinistra. Configura il bridge audio via USB; consulta la [guida USB](../../bridge_wroom/USB配置说明.md).
