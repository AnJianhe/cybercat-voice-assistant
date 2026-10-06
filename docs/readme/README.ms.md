# Pembantu Suara CyberCat

Projek rujukan pembantu suara ESP32: S3 mengendalikan pertuturan dan awan, WROOM mengendalikan audio Bluetooth, manakala C3 menyediakan ESP-NOW dan kawalan inframerah.

## Komponen

- `master_s3/`: kata bangun, pengecaman pertuturan, sembang DeepSeek dan TTS.
- `bridge_wroom/`: audio TF, PCM dan Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, pembelajaran serta penghantaran inframerah.

## Mula

Baca [konfigurasi](../configuration.md), [pendawaian](../wiring.md) dan [bina serta flash](../build-and-flash.md) dahulu. Panduan terperinci kini hanya tersedia dalam bahasa Cina Ringkas. Gunakan kelayakan awan anda sendiri; jangan siarkan kunci API, log atau rakaman.

Pusat kawalan menyokong 20 bahasa, termasuk susun atur Arab dari kanan ke kiri. Konfigurasikan jambatan audio melalui USB; lihat [panduan USB](../../bridge_wroom/USB配置说明.md).
