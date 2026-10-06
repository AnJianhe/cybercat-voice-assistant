# Asisten Suara CyberCat

Proyek referensi asisten suara ESP32: S3 menangani suara dan layanan cloud, WROOM menangani audio Bluetooth, dan C3 menyediakan ESP-NOW serta kontrol inframerah.

## Komponen

- `master_s3/`: kata pemicu, pengenalan suara, obrolan DeepSeek, dan TTS.
- `bridge_wroom/`: audio TF, PCM, dan Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, pembelajaran dan pengiriman inframerah.

## Mulai

Baca [konfigurasi](../configuration.md), [pengkabelan](../wiring.md), dan [build dan flash](../build-and-flash.md) terlebih dahulu. Panduan terperinci saat ini hanya tersedia dalam bahasa Mandarin Sederhana. Gunakan kredensial cloud sendiri; jangan publikasikan kunci API, log, atau rekaman.

Pusat kontrol mendukung 20 bahasa, termasuk tata letak Arab dari kanan ke kiri. Atur jembatan audio melalui USB; lihat [panduan USB](../../bridge_wroom/USB配置说明.md).
