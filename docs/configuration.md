# 用户需要修改的宏

| 芯片 | 宏定义 | 示例 | 说明 |
|---|---|---|---|
| S3 | `WIFI_SSID` | `"HomeWiFi"` | 2.4 GHz 家庭 WiFi |
| S3 | `WIFI_PASSWORD` | `"..."` | WiFi 密码 |
| S3 | `DEEPSEEK_API_KEY` | `"sk-..."` | DeepSeek Chat API 密钥 |
| S3 | `DEEPSEEK_MODEL` | `"deepseek-chat"` | DeepSeek 模型名 |
| S3 | `AZURE_SPEECH_KEY` | `"..."` | Azure Speech 资源密钥 |
| S3 | `AZURE_SPEECH_REGION` | `"eastasia"` | TTS 区域 |
| S3 | `AZURE_SPEECH_RESOURCE_NAME` | `"cybercat-speech"` | STT 自定义子域的资源名，不含域名 |
| S3 | `AZURE_TTS_VOICE` | `"zh-CN-XiaoxiaoNeural"` | Azure 中文语音 |
| S3 | `MIC_I2S_BCLK_GPIO` | `4` | 实际数字麦克风 BCLK；示例值不是所有板通用 |
| S3 | `MIC_I2S_WS_GPIO` | `5` | 实际数字麦克风 WS/LRCLK |
| S3 | `MIC_I2S_DATA_IN_GPIO` | `6` | 实际数字麦克风 DATA |
| S3 | `C3_MAC_0..5` | `0x34 ...` | C3 STA MAC 六字节；对应 `C3_MAC_ADDRESS` |
| WROOM | `BLUETOOTH_SPEAKER_NAME` | `"JBL Flip 6"` | 必须与发现到的音箱名完全相同 |
| C3 | `ESPNOW_WIFI_CHANNEL` | `6` | 必须与家庭 AP 的 2.4 GHz 信道一致 |
| C3 | `RECORD_GAP_MICROS` | `12000` | 空调长状态帧的结束间隔，按原遥控器实测调整 |

位置：

- S3：将 `master_s3/main/app_config.example.h` 复制为本地 `master_s3/main/app_config.h` 后填写凭据
- 有道：将 `master_s3/main/youdao_secrets.example.h` 复制为本地 `master_s3/main/youdao_secrets.h` 后填写凭据
- WROOM：`bridge_wroom/src/config.h`
- C3：`slave_c3/src/config.h`

固定引脚宏也集中在这三个文件顶部，但除非硬件拓扑发生明确变更，不应修改题目指定的 UART、TF 和红外引脚。

凭据目前是编译期宏，适合原型调试。公开仓库只提供 `.example.h` 模板；真实 `app_config.h`、`youdao_secrets.h` 和 `sdkconfig` 已忽略。产品化时应改用安全配网、NVS 加密和 Flash Encryption；不要把填有真实密钥的配置提交到公开仓库。
