# CyberCat-spraakassistent

Referentieproject voor een ESP32-spraakassistent: S3 verzorgt spraak en cloud, WROOM Bluetooth-audio en C3 ESP-NOW en infraroodbediening.

## Onderdelen

- `master_s3/`: wekwoord, spraakherkenning, DeepSeek-chat en TTS.
- `bridge_wroom/`: TF-audio, PCM en Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, infrarood leren en verzenden.

## Aan de slag

Lees eerst [configuratie](../configuration.md), [bedrading](../wiring.md) en [bouwen en flashen](../build-and-flash.md). De uitgebreide handleidingen zijn momenteel alleen in vereenvoudigd Chinees. Gebruik eigen cloudgegevens en publiceer nooit API-sleutels, logboeken of opnamen.

Het bedieningscentrum ondersteunt 20 talen, waaronder een Arabische indeling van rechts naar links. Stel de audiobrug via USB in; zie de [USB-handleiding](../../bridge_wroom/USB配置说明.md).
