# Asistente de voz CyberCat

Proyecto de referencia de un asistente ESP32: S3 gestiona voz y nube, WROOM el audio Bluetooth y C3 ESP-NOW y el control infrarrojo.

## Componentes

- `master_s3/`: palabra de activación, reconocimiento de voz, chat DeepSeek y TTS.
- `bridge_wroom/`: audio TF, PCM y Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, aprendizaje y emisión infrarroja.

## Primeros pasos

Lee primero [configuración](../configuration.md), [cableado](../wiring.md) y [compilación y flasheo](../build-and-flash.md). Estas guías detalladas están actualmente en chino simplificado. Usa tus propias credenciales en la nube; no publiques claves API, registros ni grabaciones.

El centro de control admite 20 idiomas, incluido un diseño árabe de derecha a izquierda. Configura el puente de audio por USB; consulta la [guía USB](../../bridge_wroom/USB配置说明.md).
