# Assistant vocal CyberCat

Projet de référence d’assistant vocal ESP32 : le S3 gère la parole et le cloud, le WROOM l’audio Bluetooth et le C3 ESP-NOW ainsi que l’infrarouge.

## Composants

- `master_s3/` : mot d’activation, reconnaissance vocale, chat DeepSeek et TTS.
- `bridge_wroom/` : audio TF, PCM et Bluetooth Classic A2DP.
- `slave_c3/` : ESP-NOW, apprentissage et émission infrarouges.

## Démarrage

Consultez d’abord [configuration](../configuration.md), [câblage](../wiring.md) et [compilation et flash](../build-and-flash.md). Ces guides détaillés sont actuellement en chinois simplifié. Utilisez vos propres identifiants cloud et ne publiez jamais de clés API, journaux ou enregistrements.

Le centre de contrôle prend en charge 20 langues, dont une interface arabe de droite à gauche. Le pont audio se configure par USB ; voir le [guide de configuration USB](../../bridge_wroom/USB配置说明.md).
