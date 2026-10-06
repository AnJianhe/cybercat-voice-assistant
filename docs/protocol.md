# UART 二进制协议

链路固定为 921600 baud、8N1：S3 GPIO17 TX → WROOM GPIO16 RX；S3 GPIO18 RX ← WROOM GPIO17 TX；两端共地。

```text
AA 55 | TYPE | LEN_H LEN_L | PAYLOAD | CRC_H CRC_L
```

- `LEN`：大端、仅表示 Payload 字节数，最大 2048。
- CRC：CRC-16/CCITT-FALSE，初值 `0xFFFF`，多项式 `0x1021`，覆盖 `TYPE + LEN_H + LEN_L + PAYLOAD`，线上大端。
- 接收端逐字节状态机解析；CRC 或长度错误后重新寻找 `AA 55`。

| TYPE | 值 | Payload |
|---|---:|---|
| CMD | `0x01` | UTF-8/ASCII 命令，如 `PLAY:/music/song001.mp3` |
| PCM_START | `0x02` | 7 字节：sample rate `u32be`、bits `u8`、channels `u8`、format `u8`（0=签名 little-endian PCM） |
| PCM_DATA | `0x03` | 最多 1024 字节的 PCM 数据 |
| PCM_END | `0x04` | 空 |
| STOP | `0x05` | 空 |
| ACK | `0x06` | 文本状态，如 `OK:STOPPED` |
| ERROR | `0x07` | 文本错误，如 `ERR:FILE_NOT_FOUND` |
| TTS_TEXT | `0x08` | 旧版本：S3 把回复文字发给 Bridge 自行合成。现已改为 S3 合成音频，Bridge 收到该帧一律回复 `ERR:TTS_MOVED_TO_S3` |

`STOP` 是独立帧，绝不在 PCM 内搜索字符串。Bridge 收到后停止解码/流播放、清空输入和 A2DP Ring Buffer、复位播放状态并回复 `OK:STOPPED`。

## 语音播放（当前实现）

AI 回复的语音由 **S3** 合成并下发，Bridge 只负责重采样和推 A2DP：

1. S3 用有道 TTS 的 `format=wav` 取回 24 kHz / 16 bit / 单声道 WAV，解析 RIFF 头后取出 `data` 块。
2. S3 发 `PCM_START`（rate=`24000`、bits=`16`、channels=`1`、format=`0`），Bridge 校验通过后回 `OK:PCM_STARTED`；若当前没有蓝牙音箱连接则回 `ERR:PCM_NO_BT`。
3. S3 以最多 1024 字节的 `PCM_DATA` 逐块发送，**每块等 Bridge 确认**：Bridge 把数据放进 PCM Ring Buffer 后才回 `OK:PCM_DATA`。因此发送速率被自动锁到 A2DP 的真实消费速率，不需要额外的流控。
4. `PCM_END` 表示本次语音结束，Bridge 排空队列后回 `OK:PCM_ENDED`。`STOP` 可随时中止。

Bridge 侧把输入采样率重采样到 A2DP 的 44.1 kHz，并把单声道复制成立体声。

## 背压与错误

- S3 每块 `PCM_DATA` 等 Bridge 的 ACK 才发下一块（超时 2 s，最多重发 3 次）。实测发送速率自然收敛到 A2DP 消费速率，无需额外流控。
- Bridge 的 PCM 输入 Ring Buffer 为 4 KiB（约 85 ms/24 kHz 单声道），A2DP 输出 Ring Buffer 为 4 KiB；两者都在 `bridge_wroom/src/config.h`。
- Ring Buffer 在 500 ms 内腾不出空间时 Bridge 回复 `ERR:PCM_OVERFLOW` 并结束当前流；没有音箱连接时 `PCM_START` 直接回 `ERR:PCM_NO_BT`。
- 帧级 CRC 错误由 Bridge 回复 `ERR:CRC`，S3 侧按帧重发。
- A2DP 取数不足时补静音并累计 underflow 计数；`STATUS` 可查看计数。
