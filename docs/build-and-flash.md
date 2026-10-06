# 从空白开发板编译、烧录与验收

推荐 Windows 使用 VS Code + PlatformIO 构建两个 Arduino 工程，使用 ESP-IDF 5.4.x 官方终端构建 Master。

## 1. ESP32-C3 Slave

1. 打开 `slave_c3/`，修改 `src/config.h` 中的 `ESPNOW_WIFI_CHANNEL`。
2. 执行 `pio run -t upload -t monitor`。
3. 启动日志必须打印 `C3 STA MAC` 和 `Cyber Cat C3 ready`。
4. 串口输入 `LEARN:TV_ON`，按一次原遥控器电源键；应打印 `saved`。
5. 重启 C3 后串口输入 `TV_ON`；电视能响应即证明 NVS 持久化和红外驱动链路正常。

## 2. ESP32-WROOM-32 Audio Bridge

1. TF 卡格式化为 FAT32，放入 `/music/song001.mp3` 和 `/alarm/morning.mp3`。
2. 打开 `bridge_wroom/`，填写 `BLUETOOTH_SPEAKER_NAME`。
3. 音箱进入配对模式，执行 `pio run -t upload -t monitor`。
4. 日志应打印 `Cyber Cat Bridge ready`；TF 错误必须先处理。
5. 单独用协议测试工具向 UART2 发送合法 `STATUS`/`PLAY` 帧；应收到 ACK，音箱播放 TF 文件。

## 3. ESP32-S3 Master 应用构建

在安装 ESP-IDF 5.4.x 的终端：

```powershell
cd master_s3
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

在 `menuconfig` 中确认：

- Flash size：16 MB。
- Octal PSRAM：启用，80 MHz。
- Partition Table：Custom，文件为 `partitions.csv`。
- ESP Speech Recognition：Model in flash。
- AFE：启用。
- WakeNet：当前唤醒词为“小爱同学”，选择 `wn9_xiaoaitongxue`（`CONFIG_SR_WN_WN9_XIAOAITONGXUE=y`），关闭 `wn9_hiesp`。更改后须重新生成并烧录 `model` 分区，不能只烧录应用固件。
- MultiNet：选择中文模型（建议当前组件提供的 MultiNet7 quantized）。

然后填写 `main/app_config.h`。I2S 麦克风引脚和数据位移必须用录音波形实测确认。

## 4. ESP-SR 模型/分区烧录

首次必须执行完整烧录：

```powershell
idf.py -p COMx erase-flash
idf.py -p COMx flash monitor
```

ESP-SR 的 CMake 脚本根据 `sdkconfig` 生成 `build/srmodels/srmodels.bin`，并写入标签为 `model` 的 8 MiB 分区。启动日志必须出现模型分区成功加载，并列出 WakeNet/MultiNet 名称。只执行 `app-flash` 不会替你首次部署模型。

修改模型选择后再次执行 `idf.py flash`；只改普通应用代码且模型未变时可执行 `idf.py app-flash`。

## 5. 获取 C3 MAC

打开 C3 串口，输入 `PRINT_MAC`，记录 `C3 STA MAC`。这是 STA MAC，不是蓝牙 MAC。

## 6. 修改 Master MAC

把六字节写入 `master_s3/main/app_config.h` 的 `C3_MAC_0..5`，然后：

```powershell
idf.py -p COMx app-flash monitor
```

Master 日志应显示 ESP-NOW peer ready 及当前 WiFi channel。说固定命令后，C3 串口应出现 `AC_ON`/`TV_ON` 等短消息。

## 7. 蓝牙音箱配对

1. 音箱清除旧连接或进入可发现状态。
2. 确认名称与 `BLUETOOTH_SPEAKER_NAME` 完全一致。
3. 重启 WROOM，等待自动发现并连接。
4. Master 发送 `PLAY:/music/song001.mp3`，Bridge 回复 `OK:PLAYING` 且音箱出声。

## 8. 联调顺序

逐项验证，出现问题时不要跨层排查：

1. UART `STATUS`、CRC 错误和 ACK/ERROR。
2. 本地 MP3 的 PLAY/PAUSE/RESUME/VOL/STOP。
3. Master 发送一段已知 16 kHz PCM，查看 `UNDERFLOW/OVERFLOW`。
4. WakeNet 唤醒；播放中再次说唤醒词，应立即 STOP。
5. MultiNet 固定命令 → ESP-NOW → C3 → IR。
6. Azure STT 单独成功，再验证 DeepSeek SSE，最后验证 Azure TTS PCM。
7. 设置一分钟后的闹钟，先断网验证 TF 本地兜底，再联网验证动态叫醒文本。

## ESP-SR 必需的额外配置

`master_s3` 不能仅凭一个 `.c` 文件部署语音模型。需要：

- ESP-IDF 5.4.x 工具链；
- Component Manager 下载锁定的 ESP-SR 2.5.1；
- `menuconfig` 选择 WakeNet/MultiNet；
- 自定义 `partitions.csv` 的 `model` 分区；
- 完整 `idf.py flash` 生成并烧录 `srmodels.bin`。

模型菜单名称会随 ESP-SR 版本变化，所以工程锁定组件版本，文档不伪造不存在的 `sdkconfig` 符号。最终以该锁定版本实际 `menuconfig` 选项为准。
