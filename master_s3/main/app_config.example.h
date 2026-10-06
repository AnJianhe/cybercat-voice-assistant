#pragma once
#include "driver/uart.h"

/*
 * 复制本文件为 app_config.h 后填写本机配置。
 * app_config.h 已加入 .gitignore，禁止提交真实 Wi-Fi 或云服务密钥。
 */
#define WIFI_SSID                       "YOUR_2_4_GHZ_WIFI_SSID"
#define WIFI_PASSWORD                   "YOUR_WIFI_PASSWORD"
#define DEEPSEEK_API_KEY                "YOUR_DEEPSEEK_API_KEY"
#define DEEPSEEK_MODEL                  "deepseek-chat"

/* Azure Speech：开放式 ASR + raw 16 kHz PCM TTS。 */
#define AZURE_SPEECH_KEY                "YOUR_AZURE_SPEECH_KEY"
#define AZURE_SPEECH_REGION             "eastasia"
#define AZURE_SPEECH_RESOURCE_NAME      "YOUR_AZURE_RESOURCE_NAME"
#define AZURE_SPEECH_LANGUAGE           "zh-CN"
#define AZURE_TTS_VOICE                 "zh-CN-XiaoxiaoNeural"

/* 中国标准时间；其他地区按 POSIX TZ 语法修改。 */
#define SYSTEM_TIMEZONE                 "CST-8"
#define NTP_SERVER_1                    "pool.ntp.org"
#define NTP_SERVER_2                    "time.cloudflare.com"

/* I2S 数字麦克风示例引脚。请按实际麦克风和开发板确认。 */
#define MIC_I2S_BCLK_GPIO               4
#define MIC_I2S_WS_GPIO                 5
#define MIC_I2S_DATA_IN_GPIO            6
#define MIC_SAMPLE_RIGHT_SHIFT          16
#define MIC_DIAGNOSTIC_LOG              0
#define MIC_EXPORT_DIAGNOSTIC           1

/* 电脑扬声器测试模式；使用 WROOM 音频桥时设为 0。 */
#define PC_TTS_OVER_USB                 0

/* 固定 UART 引脚和格式，不要与板载设备冲突。 */
#define BRIDGE_UART_NUM                 UART_NUM_1
#define BRIDGE_UART_TX_GPIO             17
#define BRIDGE_UART_RX_GPIO             18
#define BRIDGE_UART_BAUD                921600

/* C3 STA MAC：烧录 C3 后查询 PRINT_MAC，并填入真实地址。 */
#define C3_MAC_0                        0x00
#define C3_MAC_1                        0x00
#define C3_MAC_2                        0x00
#define C3_MAC_3                        0x00
#define C3_MAC_4                        0x00
#define C3_MAC_5                        0x00

#define OPEN_UTTERANCE_MAX_SECONDS      20
#define OPEN_UTTERANCE_START_TIMEOUT_MS 6000
#define OPEN_UTTERANCE_END_SILENCE_MS   1500
#define UART_PCM_CHUNK_BYTES            1024
#define CLOUD_HTTP_TIMEOUT_MS           30000

/* 默认闹钟：首次启动写入 NVS。 */
#define DEFAULT_ALARM_HOUR              7
#define DEFAULT_ALARM_MINUTE            0
#define DEFAULT_ALARM_REPEAT_MASK       0x7F
#define DEFAULT_ALARM_MUSIC             "/music/example.mp3"
#define DEFAULT_ALARM_VOLUME            70
