# CyberCat Sesli Asistanı

ESP32 tabanlı sesli asistan referans projesi: S3 konuşma ve bulutu, WROOM Bluetooth sesini, C3 ise ESP-NOW ve kızılötesi kontrolü yönetir.

## Bileşenler

- `master_s3/`: uyandırma sözcüğü, konuşma tanıma, DeepSeek sohbeti ve TTS.
- `bridge_wroom/`: TF sesi, PCM ve Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, kızılötesi öğrenme ve gönderme.

## Başlarken

Önce [yapılandırma](../configuration.md), [kablolama](../wiring.md) ve [derleme ve yükleme](../build-and-flash.md) kılavuzlarını okuyun. Ayrıntılı kılavuzlar şu anda yalnızca Basitleştirilmiş Çince. Kendi bulut kimlik bilgilerinizi kullanın; API anahtarı, günlük veya kayıtları yayımlamayın.

Kontrol merkezi 20 dili ve Arapça sağdan sola düzeni destekler. Ses köprüsü USB üzerinden yapılandırılır; [USB kılavuzuna](../../bridge_wroom/USB配置说明.md) bakın.
