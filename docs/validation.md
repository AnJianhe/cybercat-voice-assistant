# 验证记录

验证日期：2026-08-31。

## 2026-09-20 追加：语音链路改为 PCM 直推

### 为什么改

WROOM 只有 320 KB DRAM，实测预算如下（本次启动日志实测值）：

| 阶段 | 剩余堆 |
|---|---:|
| `setup()` 起始 | 189,792 B |
| SPIFFS 挂载后 | 185,332 B（SPIFFS 占 4.5 KB） |
| TF 卡挂载后 | 142,620 B（SD/FATFS 占 42.7 KB） |
| 蓝牙启动后 | 48,912 B（Bluedroid 占 93.7 KB） |

也就是说蓝牙一开就只剩约 49 KB。此前 TTS 走「S3 传 MP3 → Bridge 存 TF → libmad 解码」，而 libmad 需要 **29,192 B 且要求一整块连续内存**（`mad_frame` 单独就 20,784 B），只留约 19 KB 给整个蓝牙栈时会出现：

- 解码器初始化失败（`free=19372 largest=18420`）；
- 强行预留 29 KB 后，Bluedroid 在建链时因分配失败走到 `fixed_queue_new` 的清理路径，触发 `assert failed: vQueueDelete`。

换 minimp3 后总占用反而更大（scratch 16 KB + 解码器 6.6 KB + 缓冲 ≈ 31.6 KB），且 16 KB scratch 放不进任何任务栈，实测栈溢出。结论：**这块板子无法同时容纳「Bluedroid + 30 KB 级 MP3 解码器 + TF 挂载」**。

### 现在的实现

- S3 用有道 `format=wav`（实测返回 24 kHz / 16 bit / 单声道，`format=pcm` 不被支持会回落成 MP3），解析 RIFF 后按 `PCM_START/DATA/END` 逐块下发，每块等 Bridge 确认。
- Bridge 不再解码：只把输入重采样到 44.1 kHz 并推 A2DP。libmad 仅保留给 TF 卡本地文件播放，按需分配，失败时回复 `ERR:MP3_NO_MEMORY`。
- 链路波特率回到原设计的 921600（24 kHz 单声道 PCM 为 384 kbps，230400 不够）。

### 本轮实测

| 项目 | 结果 | 证据 |
|---|---|---|
| S3 固件编译 | 通过 | 应用分区剩余 42% |
| WROOM 固件编译 | 通过 | RAM 20.8%（68,108 B）、Flash 60.6% |
| WROOM 内存 | 松开 | 播放期 `freeHeap=41,508`，不再有「任务创建失败」 |
| WAV 取回与解析 | 通过 | `TTS WAV 24000 Hz / 16 bit / 1 ch, PCM 108000 bytes` |
| PCM 帧 + 逐块确认 | 通过 | `OK:PCM_STARTED`、连续 `OK:PCM_DATA`、无 CRC 错误 |
| 无音箱时的失败路径 | 通过 | `ERR:PCM_NO_BT`（此前是无提示的中途 `ERR:PCM_NOT_STARTED`） |
| A2DP 实际出声 | **待测** | 测试时音箱 `IN糖3Pro` 未广播，BT 扫描返回 0 个设备 |

## 2026-09-20 第二轮：PCM 链路调优与剩余瓶颈

### 已修并验证

| 问题 | 现象 | 处理 |
|---|---|---|
| 桥端 UART 取数靠轮询 | 1031 字节一帧要 29 ms（线速应为 12 ms） | 改用 IDF 阻塞读 `uart_read_bytes`，实测降到 10–13 ms |
| S3 发送端逐块停等 | 1 KB/130 ms，只有 25 KB/s | 改为滑动窗口 + 每块确认（`PCM_ACK_WINDOW`），并保证窗口内确认不丢计数 |
| S3 读取超时取整 | `pdMS_TO_TICKS(2)` 在 100 Hz tick 下 = 0，接收任务变非阻塞轮询，触发 IDLE 看门狗并饿死 TTS 任务 | 改为 `pdMS_TO_TICKS(10)` |
| 音频路径上打日志 | 主机读串口慢时 `ESP_LOGI` 阻塞 ~137 ms，卡死 ACK 处理，整句 2 s 超时失败 | 音频路径不再写任何日志（实测复现后移除） |
| 单块出错整句失败 | 一次 CRC 或环满就中断整句 | 改为：CRC 只重发该块、确认超时只重发该块、环满只丢该块（`OK:PCM_DROPPED`） |
| 链路吞吐 | — | 自测连续发送 409600 字节 / 4826 ms = **84.9 KB/s**（≈850 kbps，符合 921600 设置） |

跑通记录：`收 106 帧/108000 字节` → `推 A2DP 396900` → `A2DP 取走 396900（844 次）`，队列排空。

### 剩余瓶颈（不在固件内）

A2DP 音箱端的取数速率极不稳定，同一版固件同一只音箱多次实测：

| 轮次 | 桥端 A2DP 请求速率 | 结果 |
|---|---:|---|
| 第 4 轮 | 184 KB/s | 实时，正常播放 |
| 第 15 轮 | 93 KB/s | 接近实时，可听 |
| 第 21 轮 | 24 KB/s | 严重缺口，只能丢块 |

44.1 kHz 立体声要求 176 KB/s；设备间 RSSI 一直处于 -60 ~ -67 dBm 的偏弱区间。结论：**音频通路的数字部分已打通，剩下的卡在蓝牙射频链路与音箱取数行为上**，需要把音箱靠近桥板（或换一只音箱/改善天线环境）再验证。

## 已执行

| 工程/项目 | 结果 | 详情 |
|---|---|---|
| WROOM Bridge | 编译成功 | PlatformIO `espressif32@6.10.0`、Arduino-ESP32 2.0.17、ESP32-A2DP 1.8.7、ESP8266Audio 1.9.9；RAM 41,148 B / 12.6%，Flash 1,277,253 B / 40.6% |
| C3 Slave | 编译成功 | PlatformIO `espressif32@6.10.0`、Arduino-ESP32 2.0.17、IRremote 4.7.1；RAM 39,324 B / 12.0%，Flash 760,154 B / 58.0% |
| UART 协议向量 | 通过 | 7 种 TYPE round-trip、CRC-16/CCITT-FALSE 标准向量 `123456789 -> 0x29B1`、单字节损坏拒绝 |
| 伪 API/占位扫描 | 通过 | 未出现 `speechToText()`、`recognizeEverything()`、TODO/FIXME 冒充实现 |
| 固定拓扑检查 | 通过 | S3/WROOM UART 17/18、921600；WROOM TF 5/18/19/23；C3 IR 6/7；ESP-NOW 无音频路径 |

运行协议测试：

```powershell
python tests/test_protocol.py
```

## 尚需额外配置/硬件验证

- Master 未在当前电脑编译：当前环境没有 ESP-IDF 5.4.x 和约 150 MB 的 ESP-SR 2.5.1 managed component/toolchain。其 API 已按 Espressif 官方 ESP-SR 示例和 ESP-IDF 5.4 文档核对，但必须在安装完整工具链后运行 `idf.py build`。
- ESP-SR WakeNet/MultiNet 模型必须通过 `menuconfig` 选择，再由完整 `idf.py flash` 烧录 `srmodels.bin`。
- 数字麦克风的实际 I2S 引脚、左右声道和 `MIC_SAMPLE_RIGHT_SHIFT` 必须看具体模块并上板测波形。
- 蓝牙配对、A2DP 连续播放、TF 卡信号完整性、921600 UART 长线稳定性、红外晶体管驱动和不同空调的 raw 状态帧必须上板测试。
- Azure Speech 和 DeepSeek 需要真实密钥、可达网络和有效配额，不能离线模拟为“已成功”。

这些项目均在文档中标为“需要额外配置”，不声称未经硬件验证的功能已经完成。
