# 接线表

所有设备断电接线。S3 与 WROOM 必须共地；两者都是 3.3 V GPIO，不加电平转换。

## S3 ↔ WROOM UART

| ESP32-S3 | 方向 | ESP32-WROOM-32 |
|---|---:|---|
| GPIO17 TX | → | GPIO16 RX |
| GPIO18 RX | ← | GPIO17 TX |
| GND | ↔ | GND |

格式固定为 921600 baud、8N1。

## WROOM ↔ TF 卡（VSPI）

| TF 模块 | ESP32-WROOM-32 |
|---|---:|
| CS | GPIO5 |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| GND | GND |
| VCC | 使用适配模块标称电源；裸卡为 3.3 V |

TF 卡使用 FAT32，建议先创建：

```text
/music/song001.mp3
/alarm/morning.mp3
```

## S3 ↔ C3

ESP-NOW 无实体数据线。两块板各自供电，工作在同一 2.4 GHz WiFi Channel。S3 连接家庭 AP 后跟随 AP 信道；C3 的 `ESPNOW_WIFI_CHANNEL` 必须设成同一信道。

## C3 ↔ IR TX

| ESP32-C3 | 外围 |
|---|---|
| GPIO6 | 约 1 kΩ → NPN 基极（或合适逻辑 MOSFET 栅极） |
| 晶体管集电极/漏极 | 红外 LED 阴极 |
| 红外 LED 阳极 | 电源经限流电阻（按 LED 峰值电流计算） |
| 晶体管发射极/源极 | GND |

不要用 GPIO6 直接驱动高峰值红外 LED。电源与 C3 共地。

## C3 ↔ IR RX（1838/VS1838B）

| 1838 引脚 | ESP32-C3 |
|---|---|
| OUT | GPIO7 |
| GND | GND |
| VCC | 按模块规格供电；确认 OUT 不超过 3.3 V |

接收模块附近建议放置 100 nF 去耦电容。不同厂家的 1838 引脚顺序可能不同，必须看所购模块丝印/数据手册。

