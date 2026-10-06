#pragma once

/* Reuse the local-only Wi-Fi and Youdao credentials already configured for S3. */
#include "../../master_s3/main/app_config.h"
#include "../../master_s3/main/youdao_secrets.h"

/* ---------- USB 配置与持久日志 ---------- */
#define BRIDGE_MAX_BT_DEVICES       20
#define BRIDGE_LOG_MAX_BYTES        (128U * 1024U)
#define BRIDGE_LOG_FILE             "/bridge.log"
#define BRIDGE_LOG_OLD_FILE         "/bridge.old.log"

/* 固定 UART：WROOM GPIO16 RX <- S3 GPIO17 TX；GPIO17 TX -> S3 GPIO18 RX。 */
#define LINK_UART_RX_GPIO           16
#define LINK_UART_TX_GPIO           17
/* PCM 语音流需要 ~48 KB/s（24 kHz 单声道 16 bit），远高于 MP3 缓存方案。 */
#define LINK_UART_BAUD              921600

/* 固定 TF 卡 VSPI 引脚。 */
#define TF_CS_GPIO                  5
#define TF_SCK_GPIO                 18
#define TF_MISO_GPIO                19
#define TF_MOSI_GPIO                23

#define PCM_INPUT_RING_BYTES        (8 * 1024)
#define A2DP_OUTPUT_RING_BYTES      (4 * 1024)
#define FRAME_MAX_PAYLOAD           1024
#define MP3_INPUT_RING_BYTES        (16 * 1024)
#define DEFAULT_VOLUME_PERCENT      70


#define YOUDAO_TTS_URL              "https://openapi.youdao.com/ttsapi"
#define YOUDAO_TTS_VOICE            "youxiaoqin"
#define TTS_WIFI_CONNECT_TIMEOUT_MS 20000
#define TTS_HTTP_TIMEOUT_MS         30000
