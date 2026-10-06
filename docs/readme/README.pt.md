# Assistente de voz CyberCat

Projeto de referência de assistente ESP32: o S3 cuida da voz e da nuvem, o WROOM do áudio Bluetooth e o C3 do ESP-NOW e do infravermelho.

## Componentes

- `master_s3/`: palavra de ativação, reconhecimento de fala, chat DeepSeek e TTS.
- `bridge_wroom/`: áudio TF, PCM e Bluetooth Classic A2DP.
- `slave_c3/`: ESP-NOW, aprendizagem e transmissão infravermelha.

## Primeiros passos

Leia [configuração](../configuration.md), [ligações](../wiring.md) e [compilação e gravação](../build-and-flash.md). Esses guias detalhados estão atualmente em chinês simplificado. Use suas próprias credenciais de nuvem; nunca publique chaves API, logs ou gravações.

A central de controle oferece 20 idiomas, incluindo layout árabe da direita para a esquerda. Configure a ponte de áudio por USB; consulte o [guia de configuração USB](../../bridge_wroom/USB配置说明.md).
