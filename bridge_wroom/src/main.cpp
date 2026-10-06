#include <Arduino.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BluetoothA2DPSource.h>
#include <AudioFileSource.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceSPIFFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutput.h>
#include "freertos/ringbuf.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "mbedtls/sha256.h"
#include "esp_bt.h"
#include <time.h>

#include "config.h"
#include "frame_protocol.h"
#include "bridge_config.h"
#include "youdao_root_ca.h"

enum class PlayMode : uint8_t { Idle, Mp3, Pcm, TestTone };

static HardwareSerial LinkSerial(2);
static SPIClass TfSpi(VSPI);
static BluetoothA2DPSource A2dp;
static RingbufHandle_t PcmInputRing;
static RingbufHandle_t A2dpOutputRing;
static SemaphoreHandle_t StateMutex;
static SemaphoreHandle_t UartTxMutex;
static portMUX_TYPE CounterMux = portMUX_INITIALIZER_UNLOCKED;
static volatile PlayMode Mode = PlayMode::Idle;
static volatile bool Paused = false;
static volatile bool PcmEnded = false;
static volatile bool PcmCompleted = false;
static volatile bool BluetoothPriorityAbort = false;
static volatile bool TtsBluetoothHandoff = false;
static volatile bool CachedTtsPlaying = false;
static volatile bool UartMp3Receiving = false;
/* TTS 现在以 PCM 流到达（S3 直接取有道 WAV），码率由对端在 PCM_START 里声明。 */
static volatile uint32_t PcmInputRate = 16000;
/* 播放链路的可观测计数：收到多少 PCM、推给 A2DP 多少、A2DP 实际取走多少。 */
static volatile uint32_t PcmFramesIn = 0, PcmBytesIn = 0;
static volatile uint32_t PcmBytesPushed = 0;
static volatile uint32_t A2dpPulls = 0, A2dpBytesOut = 0;
/* 区分「音箱要得多快」和「我们给得多快」：requested 是 A2DP 栈要的字节数。 */
static volatile uint32_t A2dpCalls = 0, A2dpRequested = 0, A2dpFullWaits = 0;
static bool A2dpPullLogged = false;
static volatile uint32_t ToneFramesRemaining = 0, ToneBytes = 0;
static uint32_t TonePhase = 0, ToneStartedMs = 0;
static uint32_t PcmReportAtMs = 0;
static File UartMp3File;
static bool TfMounted = false;
static volatile size_t A2dpQueuedBytes = 0;
static volatile uint32_t Underflows = 0, Overflows = 0, BadCrc = 0;
static AudioFileSource *Mp3File = nullptr;
static AudioFileSource *TtsSource = nullptr;
/* libmad is only used for local files on the TF card.  Its ~29 KB working set
   is bigger than this board can keep reserved next to Bluedroid, so it is
   allocated on demand: TTS now arrives as PCM (see FRAME_PCM_*). */
static AudioGeneratorMP3 *Mp3 = nullptr;
static WiFiClientSecure TtsTls;
static HTTPClient TtsHttp;
/* UART writes the newest request here; only ttsTask performs network I/O. */
static String PendingTtsText;
static String ActiveTtsText;
static uint8_t TtsRetryCount = 0;
static uint32_t TtsRetryAtMs = 0;
static SemaphoreHandle_t TtsTextMutex;
static FrameParser LinkFrameParser;
static UartFrame LinkFrame;
static uint8_t UartTxBuffer[FRAME_MAX_PAYLOAD + 7];
/* Keep USB command assembly off the heap.  A failed TLS connection must not
   leave a fragmented String allocation for the serial console to dereference. */
static char UsbConfigBuffer[FRAME_MAX_PAYLOAD + 5];
static size_t UsbConfigLength = 0;

class HttpMp3Source final : public AudioFileSource {
public:
    HttpMp3Source(WiFiClient *client, int length)
        : client_(client), size_(length > 0 ? static_cast<uint32_t>(length) : 0),
          remaining_(length), open_(client != nullptr) {}
    uint32_t read(void *data, uint32_t length) override {
        if (!open_ || !client_) return 0;
        if (remaining_ == 0) { open_ = false; return 0; }
        uint32_t target = length;
        if (remaining_ > 0 && target > static_cast<uint32_t>(remaining_)) target = remaining_;
        uint8_t *out = static_cast<uint8_t *>(data);
        uint32_t used = 0, idleSince = millis();
        while (used < target) {
            int available = client_->available();
            if (available > 0) {
                int remaining = static_cast<int>(target - used);
                int wanted = available < remaining ? available : remaining;
                int received = client_->read(out + used, wanted);
                if (received > 0) {
                    used += received;
                    idleSince = millis();
                    continue;
                }
            }
            if (!client_->connected() || millis() - idleSince > TTS_HTTP_TIMEOUT_MS) {
                failed_ = remaining_ > 0 || millis() - idleSince > TTS_HTTP_TIMEOUT_MS;
                break;
            }
            delay(1);
        }
        pos_ += used;
        if (remaining_ > 0) remaining_ -= used;
        if (remaining_ == 0 || (!client_->connected() && client_->available() == 0)) open_ = false;
        return used;
    }
    uint32_t readNonBlock(void *data, uint32_t length) override {
        if (!open_ || !client_) return 0;
        int available = client_->available();
        if (available <= 0) return 0;
        uint32_t target = length < static_cast<uint32_t>(available) ?
                          length : static_cast<uint32_t>(available);
        if (remaining_ > 0 && target > static_cast<uint32_t>(remaining_)) target = remaining_;
        int received = client_->read(static_cast<uint8_t *>(data), target);
        if (received <= 0) return 0;
        pos_ += received;
        if (remaining_ > 0) remaining_ -= received;
        return received;
    }
    bool seek(int32_t, int) override { return false; }
    bool close() override { open_ = false; return true; }
    bool isOpen() override { return open_; }
    uint32_t getSize() override { return size_; }
    uint32_t getPos() override { return pos_; }
    bool failed() const { return failed_; }
private:
    WiFiClient *client_ = nullptr;
    uint32_t size_ = 0, pos_ = 0;
    int remaining_ = -1;
    bool open_ = false;
    bool failed_ = false;
};

static void changeQueuedBytes(int delta) {
    portENTER_CRITICAL(&CounterMux);
    if (delta >= 0) A2dpQueuedBytes += static_cast<size_t>(delta);
    else {
        size_t remove = static_cast<size_t>(-delta);
        A2dpQueuedBytes -= (remove < A2dpQueuedBytes) ? remove : A2dpQueuedBytes;
    }
    portEXIT_CRITICAL(&CounterMux);
}

static bool sendA2dpBytes(const void *data, size_t bytes, TickType_t wait = pdMS_TO_TICKS(20)) {
    if (!A2dpOutputRing || !data || bytes == 0) return false;
    /* 队列暂满是背压，不等于已经丢音频；实际放弃帧才计入溢出。 */
    /* 先记账再发布：消费者可能在 send 返回前取走数据，否则会留下虚假队列余量。 */
    changeQueuedBytes(static_cast<int>(bytes));
    if (xRingbufferSend(A2dpOutputRing, data, bytes, wait) != pdTRUE) {
        changeQueuedBytes(-static_cast<int>(bytes));
        return false;
    }
    return true;
}

class A2dpRingOutput final : public AudioOutput {
public:
    bool SetRate(int hz) override { inputRate_ = hz; phase_ = 0; return hz >= 8000 && hz <= 48000; }
    bool SetChannels(int channels) override { inputChannels_ = channels; return channels == 1 || channels == 2; }
    bool begin() override { phase_ = 0; usedFrames_ = 0; return true; }
    bool stop() override { flush(); return true; }
    void flush() override {
        if (usedFrames_) { sendA2dpBytes(buffer_, usedFrames_ * 4); usedFrames_ = 0; }
    }
    bool ConsumeSample(int16_t sample[2]) override {
        if (Mode != PlayMode::Mp3) return false;
        int16_t left = sample[0], right = inputChannels_ == 1 ? sample[0] : sample[1];
        phase_ += 44100;
        while (phase_ >= static_cast<uint32_t>(inputRate_)) {
            phase_ -= inputRate_;
            buffer_[usedFrames_ * 2] = left;
            buffer_[usedFrames_ * 2 + 1] = right;
            if (++usedFrames_ == FramesPerBlock) {
                if (!sendA2dpBytes(buffer_, sizeof(buffer_))) return false;
                usedFrames_ = 0;
            }
        }
        return true;
    }
private:
    static constexpr size_t FramesPerBlock = 128;
    int inputRate_ = 44100, inputChannels_ = 2;
    uint32_t phase_ = 0;
    size_t usedFrames_ = 0;
    int16_t buffer_[FramesPerBlock * 2]{};
};
static A2dpRingOutput DecoderOutput;

static void drainRing(RingbufHandle_t ring) {
    size_t size = 0;
    while (void *item = xRingbufferReceive(ring, &size, 0)) vRingbufferReturnItem(ring, item);
}

static void sendFrame(FrameType type, const char *text) {
    uint16_t length = text ? strlen(text) : 0;
    if (xSemaphoreTake(UartTxMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        size_t bytes = encodeFrame(type, text, length, UartTxBuffer, sizeof(UartTxBuffer));
        LinkSerial.write(UartTxBuffer, bytes); LinkSerial.flush(); xSemaphoreGive(UartTxMutex);
    }
}
static void ack(const String &text) { sendFrame(FRAME_ACK, text.c_str()); }
static void fail(const String &text) { sendFrame(FRAME_ERROR, text.c_str()); }
static void startMp3(const char *path);

static void reportTtsResult(const char *result, int httpStatus = 0, int bytes = 0) {
    Serial.printf("@CFG TTS_RESULT %s http=%d bytes=%d\n", result, httpStatus, bytes);
    DeviceConfig.log("语音合成", "结果=%s，HTTP=%d，音频字节=%d", result, httpStatus, bytes);
}

static bool sendS3CommandFromUsb(const char *command) {
    if (!command || !*command || !UartTxMutex) return false;
    sendFrame(FRAME_CMD, command);
    return true;
}

static bool ensureWifi() {
    bool connected = DeviceConfig.ensureWifi(TTS_WIFI_CONNECT_TIMEOUT_MS);
    if (!connected) DeviceConfig.log("音频桥", "Wi‑Fi 连接失败，无法请求有道语音合成");
    return connected;
}

static bool ensureClock() {
    time_t now = time(nullptr);
    uint32_t started = millis();
    while (now < 1704067200 && millis() - started < 15000) {
        delay(100);
        now = time(nullptr);
    }
    return now >= 1704067200;
}

static size_t utf8Count(const String &text) {
    size_t count = 0;
    for (size_t i = 0; i < text.length(); ++i)
        if ((static_cast<uint8_t>(text[i]) & 0xc0) != 0x80) ++count;
    return count;
}

static size_t utf8Offset(const String &text, size_t codepoints) {
    size_t count = 0;
    for (size_t i = 0; i < text.length(); ++i) {
        if ((static_cast<uint8_t>(text[i]) & 0xc0) != 0x80) {
            if (count == codepoints) return i;
            ++count;
        }
    }
    return text.length();
}

static String signingInput(const String &text) {
    size_t count = utf8Count(text);
    if (count <= 20) return text;
    size_t firstEnd = utf8Offset(text, 10);
    size_t lastStart = utf8Offset(text, count - 10);
    return text.substring(0, firstEnd) + String(count) + text.substring(lastStart);
}

static String urlEncode(const String &text) {
    static const char hex[] = "0123456789ABCDEF";
    String out;
    out.reserve(text.length() * 3);
    for (size_t i = 0; i < text.length(); ++i) {
        uint8_t ch = static_cast<uint8_t>(text[i]);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out += static_cast<char>(ch);
        } else {
            out += '%'; out += hex[ch >> 4]; out += hex[ch & 15];
        }
    }
    return out;
}

static String makeSalt() {
    uint32_t r[4] = {esp_random(), esp_random(), esp_random(), esp_random()};
    char value[37];
    snprintf(value, sizeof(value), "%08lx-%04lx-%04lx-%04lx-%04lx%08lx",
             static_cast<unsigned long>(r[0]), static_cast<unsigned long>(r[1] & 0xffff),
             static_cast<unsigned long>((r[1] >> 16) & 0xffff),
             static_cast<unsigned long>(r[2] & 0xffff),
             static_cast<unsigned long>((r[2] >> 16) & 0xffff),
             static_cast<unsigned long>(r[3]));
    return String(value);
}

static String sha256Hex(const String &input) {
    uint8_t digest[32];
    if (mbedtls_sha256_ret(reinterpret_cast<const unsigned char *>(input.c_str()),
                           input.length(), digest, 0) != 0) return String();
    char hex[65];
    for (size_t i = 0; i < sizeof(digest); ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return String(hex);
}

static void stopPlayback(bool reply) {
    xSemaphoreTake(StateMutex, portMAX_DELAY);
    Mode = PlayMode::Idle; Paused = false; PcmEnded = false; PcmCompleted = false;
    ToneFramesRemaining = 0;
    if (Mp3) { if (Mp3->isRunning()) Mp3->stop(); delete Mp3; Mp3 = nullptr; }
    if (Mp3File) { Mp3File->close(); delete Mp3File; Mp3File = nullptr; }
    if (TtsSource) { TtsSource->close(); delete TtsSource; TtsSource = nullptr; }
    TtsHttp.end();
    drainRing(PcmInputRing); drainRing(A2dpOutputRing);
    portENTER_CRITICAL(&CounterMux); A2dpQueuedBytes = 0; portEXIT_CRITICAL(&CounterMux);
    xSemaphoreGive(StateMutex);
    DeviceConfig.requestWifiOff();
    if (reply) ack("OK:STOPPED");
}

static bool cacheTtsToSpiffs(WiFiClient *stream, int contentLength) {
    if (!stream || !SPIFFS.begin(true)) return false;
    SPIFFS.remove("/tts.mp3");
    File file = SPIFFS.open("/tts.mp3", FILE_WRITE);
    if (!file) return false;
    int remaining = contentLength;
    uint8_t buffer[1024];
    uint32_t lastData = millis();
    bool ok = true;
    while ((remaining < 0 || remaining > 0) && (millis() - lastData) < TTS_HTTP_TIMEOUT_MS) {
        int available = stream->available();
        if (available <= 0) {
            if (!stream->connected()) { ok = false; break; }
            delay(2);
            continue;
        }
        size_t wanted = static_cast<size_t>(available > static_cast<int>(sizeof(buffer)) ? sizeof(buffer) : available);
        if (remaining >= 0 && wanted > static_cast<size_t>(remaining)) wanted = static_cast<size_t>(remaining);
        int received = stream->read(buffer, wanted);
        if (received <= 0) { ok = false; break; }
        if (file.write(buffer, received) != static_cast<size_t>(received)) { ok = false; break; }
        lastData = millis();
        if (remaining >= 0) remaining -= received;
    }
    file.close();
    if (remaining > 0) ok = false;
    if (!ok) SPIFFS.remove("/tts.mp3");
    return ok;
}

static void startTts(const String &text) {
    bool bluetoothWasConnected = A2dp.is_connected();
    TtsBluetoothHandoff = bluetoothWasConnected;
    BluetoothPriorityAbort = false;
    if (bluetoothWasConnected) {
        DeviceConfig.log("语音合成", "语音合成开始，暂时断开蓝牙以独占 Wi‑Fi");
        A2dp.disconnect();
        uint32_t started = millis();
        while (A2dp.is_connected() && millis() - started < 5000) delay(20);
    }
    if (!ensureWifi()) { TtsBluetoothHandoff = false; reportTtsResult("WIFI_ERROR"); DeviceConfig.requestWifiOff(); fail("ERR:WIFI"); return; }
    if (!ensureClock()) { TtsBluetoothHandoff = false; reportTtsResult("CLOCK_ERROR"); DeviceConfig.requestWifiOff(); fail("ERR:CLOCK"); return; }

    time_t now = time(nullptr);
    String salt = makeSalt();
    String currentTime = String(static_cast<unsigned long>(now));
    String signature = sha256Hex(String(YOUDAO_APP_ID) + signingInput(text) + salt +
                                 currentTime + YOUDAO_APP_SECRET);
    if (!signature.length()) { TtsBluetoothHandoff = false; reportTtsResult("SIGN_ERROR"); DeviceConfig.requestWifiOff(); fail("ERR:SIGN"); return; }
    String body = "appKey=" + urlEncode(YOUDAO_APP_ID) +
                  "&salt=" + urlEncode(salt) +
                  "&curtime=" + currentTime +
                  "&sign=" + signature +
                  "&signType=v3&format=mp3&speed=1&volume=1&voiceName=" +
                  urlEncode(YOUDAO_TTS_VOICE) + "&q=" + urlEncode(text);

    stopPlayback(false);
    TtsTls.setCACert(YOUDAO_ROOT_CA);
    TtsTls.setTimeout(TTS_HTTP_TIMEOUT_MS / 1000);
    if (!TtsHttp.begin(TtsTls, YOUDAO_TTS_URL)) {
        TtsBluetoothHandoff = false;
        reportTtsResult("BEGIN_ERROR");
        DeviceConfig.requestWifiOff();
        fail("ERR:TTS_BEGIN");
        return;
    }
    const char *headers[] = {"Content-Type"};
    TtsHttp.collectHeaders(headers, 1);
    TtsHttp.setTimeout(TTS_HTTP_TIMEOUT_MS);
    TtsHttp.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int status = TtsHttp.POST(reinterpret_cast<uint8_t *>(const_cast<char *>(body.c_str())), body.length());
    String contentType = TtsHttp.header("Content-Type");
    if (status != HTTP_CODE_OK || !contentType.startsWith("audio/")) {
        TtsBluetoothHandoff = false;
        DeviceConfig.log("音频桥", "有道语音合成失败：HTTP=%d，类型=%s", status, contentType.c_str());
        TtsHttp.end();
        reportTtsResult("API_ERROR", status);
        DeviceConfig.requestWifiOff();
        fail("ERR:TTS_API");
        return;
    }

    int audioBytes = TtsHttp.getSize();
    WiFiClient *ttsStream = TtsHttp.getStreamPtr();
    if (!ttsStream) {
        TtsBluetoothHandoff = false;
        DeviceConfig.log("音频桥", "有道语音合成响应没有可用音频流");
        TtsHttp.end();
        reportTtsResult("STREAM_ERROR", status);
        DeviceConfig.requestWifiOff();
        fail("ERR:TTS_STREAM");
        return;
    }

    bool cached = cacheTtsToSpiffs(ttsStream, audioBytes);
    TtsHttp.end();
    DeviceConfig.stopWifi();
    TtsBluetoothHandoff = false;
    if (!cached) { reportTtsResult("CACHE_ERROR", status, audioBytes); fail("ERR:TTS_CACHE"); return; }
    if (bluetoothWasConnected) {
        DeviceConfig.log("语音合成", "语音已缓存，正在重新连接蓝牙");
        A2dp.reconnect();
        uint32_t started = millis();
        while (!A2dp.is_connected() && millis() - started < 10000) delay(50);
    }
    if (!A2dp.is_connected()) { reportTtsResult("API_OK_NO_BT", status, audioBytes); fail("ERR:BT_NOT_CONNECTED"); return; }
    CachedTtsPlaying = true;
    startMp3("/tts.mp3");
    reportTtsResult("CACHED_PLAYING", status, audioBytes);
}

static void startMp3(const char *path) {
    bool fromSpiffs = path && strcmp(path, "/tts.mp3") == 0;
    if (!fromSpiffs) CachedTtsPlaying = false;
    if ((fromSpiffs && !SPIFFS.exists(path)) || (!fromSpiffs && !SD.exists(path))) {
        fail("ERR:FILE_NOT_FOUND"); return;
    }
    stopPlayback(false);
    xSemaphoreTake(StateMutex, portMAX_DELAY);
    Mp3File = fromSpiffs ? static_cast<AudioFileSource *>(new AudioFileSourceSPIFFS(path))
                         : static_cast<AudioFileSource *>(new AudioFileSourceSD(path));
    const bool sourceOpen = Mp3File && Mp3File->isOpen();
    const uint32_t sourceBytes = sourceOpen ? Mp3File->getSize() : 0;
    Mp3 = sourceOpen ? new AudioGeneratorMP3() : nullptr;
    const bool decoderStarted = sourceOpen && Mp3 && Mp3->begin(Mp3File, &DecoderOutput);
    if (decoderStarted) Mode = PlayMode::Mp3;
    else {
        /* Running Bluetooth leaves ~30 KB of heap; libmad wants ~29 KB of it
           as one contiguous block, so local playback can legitimately fail. */
        Mode = PlayMode::Idle;
        if (fromSpiffs) CachedTtsPlaying = false;
        if (Mp3) { delete Mp3; Mp3 = nullptr; }
        if (Mp3File) { Mp3File->close(); delete Mp3File; Mp3File = nullptr; }
    }
    xSemaphoreGive(StateMutex);
    DeviceConfig.log("MP3", "路径=%s，字节=%u，数据源=%d，解码器=%d，空闲=%u，最大块=%u",
                     path, static_cast<unsigned>(sourceBytes), sourceOpen, decoderStarted,
                     static_cast<unsigned>(esp_get_free_heap_size()),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    if (decoderStarted) ack("OK:PLAYING");
    else fail(!sourceOpen ? "ERR:MP3_SOURCE" : "ERR:MP3_NO_MEMORY");
}

static void handleCommand(const char *command) {
    DeviceConfig.log("S3", "收到命令：%s", command);
    if (strncmp(command, "PLAY:", 5) == 0) startMp3(command + 5);
    else if (strcmp(command, "STOP") == 0) stopPlayback(true);
    else if (strcmp(command, "PAUSE") == 0) { Paused = true; ack("OK:PAUSED"); }
    else if (strcmp(command, "RESUME") == 0) { Paused = false; ack("OK:RESUMED"); }
    else if (strncmp(command, "VOL:", 4) == 0) {
        int value = atoi(command + 4);
        if (value < 0 || value > 100) fail("ERR:BAD_VOLUME");
        else { A2dp.set_volume(static_cast<uint8_t>((value * 127) / 100)); ack("OK:VOLUME"); }
    } else if (strcmp(command, "STATUS") == 0) {
        char status[160];
        snprintf(status, sizeof(status), "OK:MODE=%u,PAUSED=%u,QUEUED=%u,UNDERFLOW=%u,OVERFLOW=%u,CRC=%u",
                 static_cast<unsigned>(Mode), Paused, static_cast<unsigned>(A2dpQueuedBytes), Underflows, Overflows, BadCrc);
        ack(status);
    } else fail("ERR:UNKNOWN_COMMAND");
}

static bool queueTtsText(const char *text) {
    if (!text || !*text) return false;
    if (xSemaphoreTake(TtsTextMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    PendingTtsText = text;
    TtsRetryCount = 0;
    TtsRetryAtMs = 0;
    xSemaphoreGive(TtsTextMutex);
    return true;
}

static uint32_t readBe32(const uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

static void handleFrame(const UartFrame &frame) {
    if (frame.type == FRAME_STOP) { stopPlayback(true); return; }
    if (frame.type == FRAME_CMD) {
        handleCommand(reinterpret_cast<const char *>(frame.payload)); return;
    }
    if (frame.type == FRAME_TTS_TEXT) {
        fail("ERR:TTS_MOVED_TO_S3");
        return;
    }
    if (frame.type == FRAME_MP3_START) {
        if (UartMp3Receiving && UartMp3File) UartMp3File.close();
        if (!TfMounted) { fail("ERR:TF_NOT_MOUNTED"); return; }
        if (SD.exists("/uart_tts.mp3")) SD.remove("/uart_tts.mp3");
        UartMp3File = SD.open("/uart_tts.mp3", FILE_WRITE);
        if (!UartMp3File) { fail("ERR:TF_CACHE_OPEN"); return; }
        UartMp3Receiving = true;
        ack("OK:MP3_STARTED");
        return;
    }
    if (frame.type == FRAME_MP3_DATA) {
        if (!UartMp3Receiving || !UartMp3File) { fail("ERR:MP3_NOT_STARTED"); return; }
        if (UartMp3File.write(frame.payload, frame.length) != frame.length) {
            UartMp3File.close(); UartMp3Receiving = false; fail("ERR:MP3_WRITE");
        } else ack("OK:MP3_DATA");
        return;
    }
    if (frame.type == FRAME_MP3_END) {
        if (!UartMp3Receiving || !UartMp3File) { fail("ERR:MP3_NOT_STARTED"); return; }
        UartMp3File.close(); UartMp3Receiving = false;
        startMp3("/uart_tts.mp3");
        ack("OK:MP3_ENDED");
        return;
    }
    if (frame.type == FRAME_STATUS || frame.type == FRAME_ACK || frame.type == FRAME_ERROR) {
        const char *text = reinterpret_cast<const char *>(frame.payload);
        if (frame.type == FRAME_STATUS) {
            DeviceConfig.setS3Mood(text);
            DeviceConfig.log("S3", "状态灯：%s", text);
        } else {
            DeviceConfig.log("S3", "%s：%s", frame.type == FRAME_ACK ? "确认" : "错误", text);
        }
        return;
    }
    if (frame.type == FRAME_PCM_START) {
        if (Mode == PlayMode::TestTone) { fail("ERR:AUDIO_BUSY"); return; }
        uint32_t rate = frame.length == 7 ? readBe32(frame.payload) : 0;
        if (frame.length != 7 || rate < 8000 || rate > 48000 ||
            frame.payload[4] != 16 || frame.payload[5] != 1 || frame.payload[6] != 0) {
            fail("ERR:UNSUPPORTED_PCM_FORMAT"); return;
        }
        DeviceConfig.log("S3", "开始 PCM 音频流：%u Hz", static_cast<unsigned>(rate));
        if (!A2dp.is_connected()) {
            /* 没有消费端时环形缓冲几秒内必然填满，直接给出明确原因。 */
            fail("ERR:PCM_NO_OUTPUT"); return;
        }
        PcmFramesIn = 0; PcmBytesIn = 0; PcmBytesPushed = 0;
        A2dpPulls = 0; A2dpBytesOut = 0; A2dpPullLogged = false;
        A2dpCalls = 0; A2dpRequested = 0; A2dpFullWaits = 0;
        Underflows = 0; Overflows = 0; BadCrc = 0;
        PcmReportAtMs = millis() + 3000;
        DeviceConfig.log("PCM", "已收到 PCM 开始帧：%u Hz/%u 位/%u 声道，蓝牙=%s",
                         static_cast<unsigned>(rate), frame.payload[4], frame.payload[5],
                         A2dp.is_connected() ? "已连接" : "未连接");
        PcmInputRate = rate;
        stopPlayback(false); Mode = PlayMode::Pcm; PcmEnded = false; ack("OK:PCM_STARTED"); return;
    }
    if (frame.type == FRAME_PCM_DATA) {
        if (Mode != PlayMode::Pcm) { fail("ERR:PCM_NOT_STARTED"); return; }
        /* 等待环形缓冲腾出空间再确认：对端据此把发送速率锁到 A2DP 的消费速率。 */
        if ((frame.length & 1) || xRingbufferSend(PcmInputRing, frame.payload, frame.length, pdMS_TO_TICKS(3000)) != pdTRUE) {
            ++Overflows;
            fail("ERR:PCM_BACKPRESSURE");
        } else {
            ++PcmFramesIn; PcmBytesIn += frame.length;
            ack("OK:PCM_DATA");
        }
        return;
    }
    if (frame.type == FRAME_PCM_END) {
        DeviceConfig.log("S3", "PCM 音频流结束");
        if (Mode == PlayMode::Pcm) {
            PcmEnded = true; /* 排空输出后确认，避免把“收完”误报成“播放结束”。 */
            DeviceConfig.log("PCM", "统计：收 %u 帧/%u 字节，推送 %u，蓝牙请求 %u（%u 次）取走 %u，队列剩余 %u，推送受阻 %u 次，缺音=%u，溢出=%u",
                             static_cast<unsigned>(PcmFramesIn), static_cast<unsigned>(PcmBytesIn),
                             static_cast<unsigned>(PcmBytesPushed),
                             static_cast<unsigned>(A2dpRequested), static_cast<unsigned>(A2dpCalls),
                             static_cast<unsigned>(A2dpBytesOut),
                             static_cast<unsigned>(A2dpQueuedBytes), static_cast<unsigned>(A2dpFullWaits),
                             static_cast<unsigned>(Underflows), static_cast<unsigned>(Overflows));
        }
        else if (PcmCompleted) ack("OK:PCM_ENDED");
        else fail("ERR:PCM_NOT_STARTED");
        return;
    }
}

static void uartRxTask(void *) {
    bool crcError;
    /* 用 IDF 的阻塞读替代 available()+vTaskDelay(1) 轮询：轮询会让每帧
       多个字节段各等一次 1 ms tick，实测 1031 字节要 29 ms（链路本可 12 ms）。 */
    static uint8_t rxChunk[512];
    for (;;) {
        int received = uart_read_bytes(UART_NUM_2, rxChunk, sizeof(rxChunk), pdMS_TO_TICKS(10));
        for (int i = 0; i < received; ++i) {
            if (LinkFrameParser.push(rxChunk[i], LinkFrame, crcError)) {
                handleFrame(LinkFrame);
            }
            else if (crcError) { ++BadCrc; DeviceConfig.log("S3", "串口数据帧校验错误"); fail("ERR:CRC"); }
        }
    }
}

static void mp3DecodeTask(void *) {
    for (;;) {
        if (Mode == PlayMode::Mp3 && !Paused) {
            xSemaphoreTake(StateMutex, portMAX_DELAY);
            if (Mp3 && Mp3->isRunning() && !Mp3->loop()) {
                bool completedTts = TtsSource != nullptr || CachedTtsPlaying;
                bool ttsStreamFailed = TtsSource != nullptr && static_cast<HttpMp3Source *>(TtsSource)->failed();
                String retryText;
                if (ttsStreamFailed && TtsRetryCount == 0) retryText = ActiveTtsText;
                if (completedTts) ActiveTtsText = "";
                Mp3->stop(); DecoderOutput.flush();
                delete Mp3; Mp3 = nullptr;
                if (Mp3File) { Mp3File->close(); delete Mp3File; Mp3File = nullptr; }
                if (TtsSource) { TtsSource->close(); delete TtsSource; TtsSource = nullptr; }
                CachedTtsPlaying = false;
                Mode = PlayMode::Idle;
                if (completedTts) TtsHttp.end();
                xSemaphoreGive(StateMutex);
                if (completedTts) {
                    if (retryText.length()) {
                        TtsRetryCount = 1;
                        TtsRetryAtMs = millis() + 5000;
                        if (xSemaphoreTake(TtsTextMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            PendingTtsText = retryText;
                            xSemaphoreGive(TtsTextMutex);
                        }
                        DeviceConfig.log("语音合成", "音频流中断，Wi‑Fi 恢复后将重播");
                        reportTtsResult("STREAM_RETRY");
                        continue;
                    }
                    uint32_t drainStarted = millis();
                    while (A2dpQueuedBytes && millis() - drainStarted < 10000) delay(10);
                    DeviceConfig.log("音频桥", "有道语音合成播放结束");
                    DeviceConfig.requestWifiOff();
                    ActiveTtsText = "";
                    ack("OK:TTS_ENDED");
                }
                continue;
            }
            xSemaphoreGive(StateMutex);
        } else vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void processPendingTts() {
    if (TtsRetryAtMs && static_cast<int32_t>(millis() - TtsRetryAtMs) < 0) return;
    TtsRetryAtMs = 0;
    String text;
    if (xSemaphoreTake(TtsTextMutex, 0) == pdTRUE) {
        text = PendingTtsText;
        PendingTtsText = "";
        xSemaphoreGive(TtsTextMutex);
    }
    if (text.length()) {
        startTts(text);
        if (TtsRetryCount == 1 && ActiveTtsText.length() == 0) {
            TtsRetryAtMs = millis() + 5000;
            if (xSemaphoreTake(TtsTextMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                PendingTtsText = text;
                xSemaphoreGive(TtsTextMutex);
            }
        }
    }
}

static void pcmResampleTask(void *) {
    uint32_t phase = 0; int16_t out[256]; size_t outFrames = 0;
    for (;;) {
        if (Mode != PlayMode::Pcm || Paused) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        size_t bytes = 0;
        /* 只借一帧；整环借走会直到重采样数千字节后才归还，阻塞接收端。 */
        uint8_t *item = static_cast<uint8_t *>(xRingbufferReceiveUpTo(PcmInputRing, &bytes, pdMS_TO_TICKS(20), FRAME_MAX_PAYLOAD));
        if (!item) {
            if (PcmEnded && A2dpQueuedBytes == 0) {
                Mode = PlayMode::Idle; PcmEnded = false; PcmCompleted = true; phase = 0;
                ack("OK:PCM_ENDED");
                DeviceConfig.log("PCM", "播放排空：输入=%u，输出=%u，实际取走=%u，队列=%u，CRC=%u，丢帧=%u",
                    (unsigned)PcmBytesIn, (unsigned)PcmBytesPushed, (unsigned)A2dpBytesOut,
                    (unsigned)A2dpQueuedBytes, (unsigned)BadCrc, (unsigned)Overflows);
            }
            continue;
        }
        bytes &= ~static_cast<size_t>(1);
        for (size_t i = 0; i < bytes; i += 2) {
            int16_t sample = static_cast<int16_t>(item[i] | (static_cast<uint16_t>(item[i + 1]) << 8));
            phase += 44100;
            while (phase >= PcmInputRate) {
                phase -= PcmInputRate; out[outFrames * 2] = sample; out[outFrames * 2 + 1] = sample;
                if (++outFrames == 128) {
                    /* A2DP 队列拥塞属正常现象（音箱缓冲满时会短暂停收），
                       耐心等它排空；只有蓝牙真的断开才结束本次播放。 */
                    while (!sendA2dpBytes(out, sizeof(out))) {
                        ++A2dpFullWaits;
                        if (Mode != PlayMode::Pcm || !A2dp.is_connected()) { Mode = PlayMode::Idle; break; }
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    if (Mode == PlayMode::Pcm) PcmBytesPushed += sizeof(out);
                    outFrames = 0;
                    if (Mode != PlayMode::Pcm) break;
                }
            }
        }
        vRingbufferReturnItem(PcmInputRing, item);
        if (outFrames) {
            while (Mode == PlayMode::Pcm && !sendA2dpBytes(out, outFrames * 4)) {
                ++A2dpFullWaits;
                if (!A2dp.is_connected()) { Mode = PlayMode::Idle; break; }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            if (Mode == PlayMode::Pcm) PcmBytesPushed += outFrames * 4;
            outFrames = 0;
        }
    }
}

static int32_t a2dpDataCallback(uint8_t *data, int32_t length) {
    // ESP32-A2DP may issue one final callback with a null buffer while
    // disconnecting. Never dereference it during the TTS Bluetooth handoff.
    if (!data || length <= 0 || !A2dpOutputRing) return 0;
    ++A2dpCalls; A2dpRequested += static_cast<uint32_t>(length);
    memset(data, 0, length); int32_t copied = 0;
    if (Paused) return length;
    if (Mode == PlayMode::TestTone) {
        /* 直接填入蓝牙回调，不经过 UART、环形缓冲、解码器或网络。 */
        for (int32_t i = 0; i + 3 < length && ToneFramesRemaining; i += 4) {
            int16_t sample = TonePhase < 22050 ? 1800 : -1800;
            TonePhase += 440;
            if (TonePhase >= 44100) TonePhase -= 44100;
            data[i] = data[i + 2] = static_cast<uint8_t>(sample);
            data[i + 1] = data[i + 3] = static_cast<uint8_t>(sample >> 8);
            --ToneFramesRemaining;
            ToneBytes += 4;
        }
        return length;
    }
    while (copied < length) {
        size_t bytes = 0;
        uint8_t *item = static_cast<uint8_t *>(xRingbufferReceiveUpTo(A2dpOutputRing, &bytes, 0, length - copied));
        if (!item) break;
        memcpy(data + copied, item, bytes); copied += bytes; changeQueuedBytes(-static_cast<int>(bytes));
        vRingbufferReturnItem(A2dpOutputRing, item);
    }
    if (copied > 0) {
        ++A2dpPulls; A2dpBytesOut += static_cast<uint32_t>(copied);
        if (!A2dpPullLogged) {
            A2dpPullLogged = true;
            /* 蓝牙实时回调不写 SPIFFS、不打印 USB，避免阻塞协议栈。 */
        }
    }
    if (copied < length && Mode != PlayMode::Idle && !Paused) ++Underflows;
    return length; /* remaining bytes are silence */
}

static bool bluetoothDeviceCallback(const char *name, esp_bd_addr_t address, int rssi) {
    return DeviceConfig.onBluetoothDevice(name, address, rssi);
}

static void bluetoothConnectionCallback(esp_a2d_connection_state_t state, void *) {
    bool connected = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
    if (!TtsBluetoothHandoff) BluetoothPriorityAbort = !connected;
    DeviceConfig.setBluetoothConnected(connected);
}

static void startBluetoothSubsystem() {
    DeviceConfig.log("音频桥", "正在启动蓝牙扫描；可从 USB 工具读取设备列表");
    A2dp.start();
    esp_err_t btPower = esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9);
    DeviceConfig.log("音频桥", "蓝牙发射功率已设为最大（+9 dBm）：%s", btPower == ESP_OK ? "成功" : "失败");
}

static int hexNibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool decodeHex(const String &encoded, String &decoded, size_t maximumBytes) {
    decoded = "";
    if (encoded == "-") return true;
    if (!encoded.length() || (encoded.length() & 1) || encoded.length() / 2 > maximumBytes) return false;
    decoded.reserve(encoded.length() / 2);
    for (size_t index = 0; index < encoded.length(); index += 2) {
        int high = hexNibble(encoded[index]);
        int low = hexNibble(encoded[index + 1]);
        if (high < 0 || low < 0) {
            decoded = "";
            return false;
        }
        decoded += static_cast<char>((high << 4) | low);
    }
    return true;
}

static bool splitTwoArguments(const String &arguments, String &first, String &second) {
    int separator = arguments.indexOf(' ');
    if (separator <= 0) return false;
    first = arguments.substring(0, separator);
    second = arguments.substring(separator + 1);
    first.trim();
    second.trim();
    return first.length() && second.length();
}

static void configOk(const char *operation) {
    Serial.printf("@CFG OK %s\n", operation);
}

static void configError(const char *operation, const String &reason) {
    Serial.printf("@CFG ERROR %s %s\n", operation, reason.c_str());
}

static void handleUsbConfigCommand(const String &line) {
    if (line == "CFG HELLO") {
        Serial.println("@CFG HELLO CyberCat-AudioBridge 1");
        DeviceConfig.printStatus(Serial);
    } else if (line == "CFG STATUS") {
        DeviceConfig.printStatus(Serial);
    } else if (line == "CFG BT_TONE") {
        if (!A2dp.is_connected()) { configError("BT_TONE", "蓝牙音响未连接"); return; }
        if (Mode != PlayMode::Idle) { configError("BT_TONE", "正在播放，请先停止"); return; }
        stopPlayback(false);
        TonePhase = 0; ToneBytes = 0;
        A2dpCalls = 0; A2dpRequested = 0;
        ToneFramesRemaining = 44100 * 3;
        ToneStartedMs = millis();
        Mode = PlayMode::TestTone;
        configOk("BT_TONE");
        DeviceConfig.log("蓝牙自检", "开始本地测试音：440 Hz，3 秒，双声道；请确认音箱是否出声");
    } else if (line == "CFG WIFI_SCAN") {
        DeviceConfig.scanWifi(Serial);
    } else if (line.startsWith("CFG WIFI_SET ")) {
        String first, second, ssid, password, error;
        bool valid = splitTwoArguments(line.substring(strlen("CFG WIFI_SET ")), first, second) &&
                     decodeHex(first, ssid, 32) && decodeHex(second, password, 64);
        if (!valid) configError("WIFI_SET", "invalid_encoding");
        else if (DeviceConfig.saveWifi(ssid, password, error)) configOk("WIFI_SET");
        else configError("WIFI_SET", error);
        for (size_t i = 0; i < password.length(); ++i) password.setCharAt(i, '\0');
    } else if (line == "CFG BT_LIST") {
        DeviceConfig.printBluetoothDevices(Serial);
    } else if (line.startsWith("CFG BT_SELECT ")) {
        String first, mac, name, error;
        bool valid = splitTwoArguments(line.substring(strlen("CFG BT_SELECT ")), first, mac) &&
                     decodeHex(first, name, 63);
        if (!valid) configError("BT_SELECT", "invalid_encoding");
        else if (DeviceConfig.selectBluetooth(name, mac, error)) configOk("BT_SELECT_RESTARTING");
        else configError("BT_SELECT", error);
    } else if (line == "CFG BT_RESCAN") {
        String error;
        if (DeviceConfig.clearBluetoothSelection(error)) configOk("BT_RESCAN_RESTARTING");
        else configError("BT_RESCAN", error);
    } else if (line == "CFG BT_RECONNECT") {
        String error;
        if (DeviceConfig.reconnectBluetooth(error)) configOk("BT_RECONNECT_RESTARTING");
        else configError("BT_RECONNECT", error);
    } else if (line.startsWith("CFG DEEPSEEK_SET ")) {
        String key, error;
        if (!decodeHex(line.substring(strlen("CFG DEEPSEEK_SET ")), key, 127)) {
            configError("DEEPSEEK_SET", "invalid_encoding");
        } else if (DeviceConfig.updateDeepSeekKey(key, error)) {
            configOk("DEEPSEEK_SET");
        } else {
            configError("DEEPSEEK_SET", error);
        }
        for (size_t i = 0; i < key.length(); ++i) key.setCharAt(i, '\0');
    } else if (line.startsWith("CFG TTS_TEST ")) {
        String text;
        if (!decodeHex(line.substring(strlen("CFG TTS_TEST ")), text, FRAME_MAX_PAYLOAD) || !text.length()) {
            configError("TTS_TEST", "invalid_encoding");
        } else if (sendS3CommandFromUsb((String("TTS_TEST:") + text).c_str())) {
            configOk("TTS_TEST");
        } else {
            configError("TTS_TEST", "s3_link_unavailable");
        }
    } else if (line == "CFG LOG_STATUS") {
        DeviceConfig.printPersistentLogStatus(Serial);
    } else if (line == "CFG LOG_DUMP") {
        DeviceConfig.dumpPersistentLog(Serial);
    } else if (line == "CFG LOG_CLEAR") {
        if (DeviceConfig.clearPersistentLog()) configOk("LOG_CLEAR");
        else configError("LOG_CLEAR", "storage_error");
    } else if (line == "CFG RESTART") {
        configOk("RESTARTING");
        DeviceConfig.scheduleRestart(500);
    } else {
        configError("COMMAND", "unknown_command");
    }
}

static void pollSerialConsole() {
    while (Serial.available()) {
        char ch = static_cast<char>(Serial.read());
        if (ch == '\r' || ch == '\n') {
            while (UsbConfigLength && (UsbConfigBuffer[UsbConfigLength - 1] == ' ' ||
                                       UsbConfigBuffer[UsbConfigLength - 1] == '\t')) {
                --UsbConfigLength;
            }
            UsbConfigBuffer[UsbConfigLength] = '\0';
            if (UsbConfigLength >= 4 && strncmp(UsbConfigBuffer, "CFG ", 4) == 0) {
                /* The config parser currently uses String for hex decoding.  Do
                   one checked conversion only after the complete fixed buffer
                   has been assembled, rather than retaining a heap String in
                   the always-running serial task. */
                String line(UsbConfigBuffer);
                if (line.length() == UsbConfigLength) handleUsbConfigCommand(line);
                else configError("COMMAND", "no_memory");
            } else if (strcmp(UsbConfigBuffer, "LOG_DUMP") == 0) DeviceConfig.dumpPersistentLog(Serial);
            else if (strcmp(UsbConfigBuffer, "LOG_STATUS") == 0) DeviceConfig.printPersistentLogStatus(Serial);
            else if (strcmp(UsbConfigBuffer, "NET_STATUS") == 0)
                    Serial.printf("NET_STATUS connected=%u ssid=%s sta_ip=%s\n",
                                  WiFi.status() == WL_CONNECTED, WiFi.SSID().c_str(),
                                  WiFi.localIP().toString().c_str());
            else if (strcmp(UsbConfigBuffer, "LOG_CLEAR") == 0)
                Serial.println(DeviceConfig.clearPersistentLog() ? "LOG_CLEAR_OK" : "LOG_CLEAR_FAILED");
            else if (UsbConfigLength > 4 && strncmp(UsbConfigBuffer, "TTS:", 4) == 0 &&
                     queueTtsText(UsbConfigBuffer + 4))
                Serial.println("USB TTS test queued");
            UsbConfigLength = 0;
            UsbConfigBuffer[0] = '\0';
        } else if (UsbConfigLength < sizeof(UsbConfigBuffer) - 1) {
            UsbConfigBuffer[UsbConfigLength++] = ch;
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.printf("[内存] 启动起始：空闲=%u，最大块=%u\n",
                  static_cast<unsigned>(esp_get_free_heap_size()),
                  static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    DeviceConfig.beginStorage();
    DeviceConfig.log("内存", "SPIFFS 挂载后：空闲=%u，最大块=%u",
                     static_cast<unsigned>(esp_get_free_heap_size()),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    DeviceConfig.log("音频桥", "=== 启动，复位原因=%d ===", static_cast<int>(esp_reset_reason()));
    /* 发送端允许 2 块在途（约 2 KB），这里留 8 KB 接收环兜住突发。 */
    LinkSerial.setRxBufferSize(8192);
    LinkSerial.begin(LINK_UART_BAUD, SERIAL_8N1, LINK_UART_RX_GPIO, LINK_UART_TX_GPIO);
    StateMutex = xSemaphoreCreateMutex(); UartTxMutex = xSemaphoreCreateMutex();
    TtsTextMutex = xSemaphoreCreateMutex();
    PcmInputRing = xRingbufferCreate(PCM_INPUT_RING_BYTES, RINGBUF_TYPE_BYTEBUF);
    A2dpOutputRing = xRingbufferCreate(A2DP_OUTPUT_RING_BYTES, RINGBUF_TYPE_BYTEBUF);
    if (!StateMutex || !UartTxMutex || !TtsTextMutex ||
        !PcmInputRing || !A2dpOutputRing) abort();
    TfSpi.begin(TF_SCK_GPIO, TF_MISO_GPIO, TF_MOSI_GPIO, TF_CS_GPIO);
    TfMounted = SD.begin(TF_CS_GPIO, TfSpi, 20000000);
    DeviceConfig.log("内存", "TF 卡挂载后：空闲=%u，最大块=%u",
                     static_cast<unsigned>(esp_get_free_heap_size()),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    DeviceConfig.setTfMounted(TfMounted);
    if (!TfMounted) DeviceConfig.log("音频桥", "TF 卡未挂载；旧版 MP3 缓存功能不可用");
    else DeviceConfig.log("音频桥", "TF 卡已挂载；旧版 MP3 缓存位置=/uart_tts.mp3");
    DeviceConfig.setS3CommandCallback(sendS3CommandFromUsb);
    DeviceConfig.begin();
    DeviceConfig.log("内存", "配置服务启动后：空闲=%u，最大块=%u",
                     static_cast<unsigned>(esp_get_free_heap_size()),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    /* Allocate Wi-Fi/event/netif memory before the large worker task stacks.
       The classic ESP32 has a small contiguous internal-RAM margin. */
    DeviceConfig.log("音频桥", "Wi‑Fi 待机关闭；收到语音合成文字时按需启动");
    DeviceConfig.log("内存", "蓝牙启动前：空闲=%u，最低=%u，最大块=%u",
                     static_cast<unsigned>(esp_get_free_heap_size()),
                     static_cast<unsigned>(esp_get_minimum_free_heap_size()),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    A2dp.set_data_callback(a2dpDataCallback);
    A2dp.set_ssid_callback(bluetoothDeviceCallback);
    A2dp.set_on_connection_state_changed(bluetoothConnectionCallback, nullptr);
    A2dp.set_auto_reconnect(false);
    A2dp.set_volume((DEFAULT_VOLUME_PERCENT * 127) / 100);
    /* Bluedroid needs a large contiguous internal-RAM block.  Start it after
       Wi-Fi has allocated its netif/event objects, but before worker stacks
       fragment the remaining heap. */
    startBluetoothSubsystem();
    DeviceConfig.log("内存", "蓝牙启动后：空闲=%u，最低=%u，最大块=%u",
                     static_cast<unsigned>(esp_get_free_heap_size()),
                     static_cast<unsigned>(esp_get_minimum_free_heap_size()),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    const bool uartTaskOk = xTaskCreatePinnedToCore(uartRxTask, "uart_rx", 4096, nullptr, 7, nullptr, 1) == pdPASS;
    const bool mp3TaskOk = xTaskCreatePinnedToCore(mp3DecodeTask, "mp3_decode", 6144, nullptr, 5, nullptr, 0) == pdPASS;
    const bool pcmTaskOk = xTaskCreatePinnedToCore(pcmResampleTask, "pcm_resample", 4096, nullptr, 6, nullptr, 0) == pdPASS;
    DeviceConfig.log("音频桥", "任务创建：串口接收=%s，MP3 解码=%s，PCM 重采样=%s",
                     uartTaskOk ? "OK" : "失败", mp3TaskOk ? "OK" : "失败", pcmTaskOk ? "OK" : "失败");
    DeviceConfig.log("音频桥", "赛博猫猫音频桥就绪；配置入口为 USB 串口");
    Serial.println("@CFG READY 1");
}

void loop() {
    if (BluetoothPriorityAbort && !TtsBluetoothHandoff) DeviceConfig.stopWifi();
    DeviceConfig.loop();
    pollSerialConsole();
    processPendingTts();
    if (Mode == PlayMode::TestTone && (!ToneFramesRemaining || !A2dp.is_connected() || millis() - ToneStartedMs >= 10000)) {
        uint32_t elapsed = millis() - ToneStartedMs;
        Mode = PlayMode::Idle;
        bool complete = ToneFramesRemaining == 0;
        ToneFramesRemaining = 0;
        Serial.printf("@CFG BT_TONE_RESULT %s bytes=%u elapsed=%u calls=%u requested=%u\n",
            complete ? "OK" : "FAIL", (unsigned)ToneBytes, (unsigned)elapsed,
            (unsigned)A2dpCalls, (unsigned)A2dpRequested);
        DeviceConfig.log("蓝牙自检", "本地测试音结束：%s，生成=%u 字节，耗时=%u ms；数字完成不代表实物已出声",
            complete ? "完整输出" : "超时或蓝牙断开", (unsigned)ToneBytes, (unsigned)elapsed);
    }
    /* 播放期间的周期体检：能直接看出「收到 / 推给 A2DP / A2DP 取走」三段各走了多少。 */
    if (Mode == PlayMode::Pcm && PcmReportAtMs && static_cast<int32_t>(millis() - PcmReportAtMs) >= 0) {
        PcmReportAtMs = millis() + 3000;
        DeviceConfig.log("PCM", "收 %u 帧/%u 字节，推送 %u，蓝牙请求 %u（%u 次）取走 %u，队列 %u，推送受阻 %u 次，缺音=%u，溢出=%u",
                         static_cast<unsigned>(PcmFramesIn), static_cast<unsigned>(PcmBytesIn),
                         static_cast<unsigned>(PcmBytesPushed),
                         static_cast<unsigned>(A2dpRequested), static_cast<unsigned>(A2dpCalls),
                         static_cast<unsigned>(A2dpBytesOut),
                         static_cast<unsigned>(A2dpQueuedBytes), static_cast<unsigned>(A2dpFullWaits),
                         static_cast<unsigned>(Underflows), static_cast<unsigned>(Overflows));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
