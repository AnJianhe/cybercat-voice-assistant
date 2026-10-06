#pragma once

/* ---------- 用户配置：集中修改此文件 ---------- */
/* 必须与家庭 AP 当前信道一致；Master STA 会跟随 AP，C3 必须设置同一信道。 */
#define ESPNOW_WIFI_CHANNEL     4

/* 固定红外引脚。GPIO6 必须通过晶体管/MOSFET 驱动红外 LED。 */
#define IR_TX_GPIO              6
#define IR_RX_GPIO              7
#define IR_CARRIER_KHZ          38

#define LEARN_TIMEOUT_MS        30000
/* 部分空调状态帧的段间隔较长；若学习结果仍被切断，应按遥控器实测调整。 */
#define RECORD_GAP_MICROS       12000
#define RAW_BUFFER_LENGTH       750
#define USE_16_BIT_TIMING_BUFFER
#define NO_LED_FEEDBACK_CODE
