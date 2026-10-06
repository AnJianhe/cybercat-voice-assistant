#include "bridge_config.h"

#include <SPIFFS.h>
#include <WiFi.h>
#include <cstdarg>
#include <cstring>
#include <time.h>

BridgeConfig DeviceConfig;

bool BridgeConfig::beginStorage() {
    if (storageReady_) return true;
    if (!storageLock_) storageLock_ = xSemaphoreCreateMutex();
    if (!storageLock_) return false;
    storageReady_ = SPIFFS.begin(true);
    if (!storageReady_) Serial.println("[bridge] 持久化日志存储挂载失败");
    return storageReady_;
}

void BridgeConfig::appendPersistentLog(uint32_t ms, const char *source, const char *text) {
    if (!storageReady_ || !storageLock_) return;
    char line[232];
    int length = snprintf(line, sizeof(line), "[%lu][%s] %s\n",
                          static_cast<unsigned long>(ms), source ? source : "bridge", text ? text : "");
    if (length <= 0) return;
    size_t bytes = static_cast<size_t>(length < static_cast<int>(sizeof(line)) ? length : sizeof(line) - 1);
    if (xSemaphoreTake(storageLock_, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    File current = SPIFFS.open(BRIDGE_LOG_FILE, FILE_READ);
    size_t currentSize = current ? current.size() : 0;
    if (current) current.close();
    if (currentSize + bytes > BRIDGE_LOG_MAX_BYTES) {
        SPIFFS.remove(BRIDGE_LOG_OLD_FILE);
        SPIFFS.rename(BRIDGE_LOG_FILE, BRIDGE_LOG_OLD_FILE);
    }
    File file = SPIFFS.open(BRIDGE_LOG_FILE, FILE_APPEND);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(line), bytes);
        file.flush();
        file.close();
    }
    xSemaphoreGive(storageLock_);
}

void BridgeConfig::dumpPersistentLog(Stream &output) {
    if (!storageReady_ || !storageLock_) {
        output.println("日志读取失败：存储尚未就绪");
        return;
    }
    if (xSemaphoreTake(storageLock_, pdMS_TO_TICKS(3000)) != pdTRUE) {
        output.println("日志读取失败：存储正在使用");
        return;
    }
    output.println("日志读取开始");
    const char *paths[] = {BRIDGE_LOG_OLD_FILE, BRIDGE_LOG_FILE};
    uint8_t buffer[256];
    for (const char *path : paths) {
        if (!SPIFFS.exists(path)) continue;
        File file = SPIFFS.open(path, FILE_READ);
        while (file && file.available()) {
            size_t count = file.read(buffer, sizeof(buffer));
            if (count) output.write(buffer, count);
        }
        if (file) file.close();
    }
    output.println("日志读取结束");
    xSemaphoreGive(storageLock_);
}

void BridgeConfig::printPersistentLogStatus(Stream &output) {
    if (!storageReady_ || !storageLock_) {
        output.println("日志存储状态：未就绪");
        return;
    }
    if (xSemaphoreTake(storageLock_, pdMS_TO_TICKS(1000)) != pdTRUE) {
        output.println("日志存储状态：就绪但正在使用");
        return;
    }
    File current = SPIFFS.exists(BRIDGE_LOG_FILE) ? SPIFFS.open(BRIDGE_LOG_FILE, FILE_READ) : File();
    File old = SPIFFS.exists(BRIDGE_LOG_OLD_FILE) ? SPIFFS.open(BRIDGE_LOG_OLD_FILE, FILE_READ) : File();
    output.printf("日志存储状态：就绪，当前=%u，旧日志=%u，已用=%u，总量=%u，单文件上限=%u\n",
                  current ? static_cast<unsigned>(current.size()) : 0,
                  old ? static_cast<unsigned>(old.size()) : 0,
                  static_cast<unsigned>(SPIFFS.usedBytes()), static_cast<unsigned>(SPIFFS.totalBytes()),
                  static_cast<unsigned>(BRIDGE_LOG_MAX_BYTES));
    if (current) current.close();
    if (old) old.close();
    xSemaphoreGive(storageLock_);
}

bool BridgeConfig::clearPersistentLog() {
    if (!storageReady_ || !storageLock_) return false;
    if (xSemaphoreTake(storageLock_, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
    bool result = true;
    if (SPIFFS.exists(BRIDGE_LOG_FILE)) result = SPIFFS.remove(BRIDGE_LOG_FILE) && result;
    if (SPIFFS.exists(BRIDGE_LOG_OLD_FILE)) result = SPIFFS.remove(BRIDGE_LOG_OLD_FILE) && result;
    xSemaphoreGive(storageLock_);
    return result;
}

String BridgeConfig::jsonEscape(const char *value) {
    String out;
    if (!value) return out;
    while (*value) {
        unsigned char c = static_cast<unsigned char>(*value++);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c >= 0x20) out += static_cast<char>(c);
    }
    return out;
}

void BridgeConfig::log(const char *source, const char *format, ...) {
    char text[176];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    Serial.printf("[%s] %s\n", source, text);
    uint32_t now = millis();
    appendPersistentLog(now, source, text);
}

void BridgeConfig::connectSavedWifi() {
    char ssid[sizeof(wifiSsid_)];
    char password[sizeof(wifiPassword_)];
    portENTER_CRITICAL(&lock_);
    strlcpy(ssid, wifiSsid_, sizeof(ssid));
    strlcpy(password, wifiPassword_, sizeof(password));
    portEXIT_CRITICAL(&lock_);
    if (!ssid[0]) return;
    WiFi.begin(ssid, password);
    lastWifiAttemptMs_ = millis();
    log("音频桥", "正在连接 Wi‑Fi：%s", ssid);
    memset(password, 0, sizeof(password));
}

void BridgeConfig::begin() {
    beginStorage();
    preferences_.begin("cybercat", false);
    String savedSsid = preferences_.getString("wifi_ssid", WIFI_SSID);
    String savedPassword = preferences_.getString("wifi_pass", WIFI_PASSWORD);
    String savedBtName = preferences_.getString("bt_name", "");
    String savedBtMac = preferences_.getString("bt_mac", "");
    strlcpy(wifiSsid_, savedSsid.c_str(), sizeof(wifiSsid_));
    strlcpy(wifiPassword_, savedPassword.c_str(), sizeof(wifiPassword_));
    strlcpy(selectedBtName_, savedBtName.c_str(), sizeof(selectedBtName_));
    strlcpy(selectedBtMac_, savedBtMac.c_str(), sizeof(selectedBtMac_));

    log("音频桥", "网页和配置热点已关闭；请使用 USB 图形配置工具");
}

void BridgeConfig::startWifi() {
    if (wifiStarted_) return;
    wifiOffAtMs_ = 0;
    WiFi.mode(WIFI_STA);
    wifiStarted_ = true;
    connectSavedWifi();
    delay(250);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
}

void BridgeConfig::stopWifi() {
    if (!wifiStarted_) return;
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    wifiStarted_ = false;
    wifiOffAtMs_ = 0;
    lastWifiConnected_ = false;
    log("音频桥", "Wi‑Fi 已关闭（空闲省电）");
}

void BridgeConfig::requestWifiOff(uint32_t delayMs) {
    if (!wifiStarted_) return;
    wifiOffAtMs_ = millis() + delayMs;
}

void BridgeConfig::cancelWifiOff() {
    wifiOffAtMs_ = 0;
}

bool BridgeConfig::ensureWifi(uint32_t timeoutMs) {
    if (!wifiStarted_) startWifi();
    cancelWifiOff();
    if (WiFi.status() == WL_CONNECTED) return true;
    if (wifiSsid_[0] && millis() - lastWifiAttemptMs_ > 1000) connectSavedWifi();
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) delay(100);
    return WiFi.status() == WL_CONNECTED;
}

void BridgeConfig::loop() {
    uint32_t wifiOffAt = wifiOffAtMs_;
    if (wifiStarted_ && wifiOffAt && static_cast<int32_t>(millis() - wifiOffAt) >= 0) {
        stopWifi();
        return;
    }
    if (!wifiStarted_) {
        uint32_t restartAt = restartAtMs_;
        if (restartAt && static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
        return;
    }
    bool connected = WiFi.status() == WL_CONNECTED;
    if (connected != lastWifiConnected_) {
        lastWifiConnected_ = connected;
        if (connected) {
            log("音频桥", "Wi‑Fi 已连接：%s，IP=%s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
        } else {
            log("音频桥", "Wi‑Fi 已断开；USB 配置仍可使用");
        }
    }
    if (!connected && wifiSsid_[0] && millis() - lastWifiAttemptMs_ > 15000) connectSavedWifi();
    uint32_t restartAt = restartAtMs_;
    if (restartAt && static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
}

void BridgeConfig::clearBluetoothDevices() {
    portENTER_CRITICAL(&lock_);
    btCount_ = 0;
    memset(btDevices_, 0, sizeof(btDevices_));
    portEXIT_CRITICAL(&lock_);
}

bool BridgeConfig::onBluetoothDevice(const char *name, const uint8_t address[6], int rssi) {
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             address[0], address[1], address[2], address[3], address[4], address[5]);
    bool isNew = false;
    bool selected = false;
    portENTER_CRITICAL(&lock_);
    size_t index = btCount_;
    for (size_t i = 0; i < btCount_; ++i) {
        if (strcmp(btDevices_[i].mac, mac) == 0) {
            index = i;
            break;
        }
    }
    if (index == btCount_ && btCount_ < BRIDGE_MAX_BT_DEVICES) {
        ++btCount_;
        isNew = true;
    }
    if (index < BRIDGE_MAX_BT_DEVICES) {
        strlcpy(btDevices_[index].name, (name && *name) ? name : "未命名设备", sizeof(btDevices_[index].name));
        strlcpy(btDevices_[index].mac, mac, sizeof(btDevices_[index].mac));
        btDevices_[index].rssi = rssi;
        btDevices_[index].seenMs = millis();
    }
    selected = selectedBtMac_[0] && strcasecmp(selectedBtMac_, mac) == 0;
    portEXIT_CRITICAL(&lock_);
    if (isNew) log("音频桥", "发现蓝牙音响：%s [%s]，信号=%d", name && *name ? name : "未命名设备", mac, rssi);
    if (selected) log("音频桥", "正在连接所选蓝牙音响：%s [%s]", name && *name ? name : selectedBtName_, mac);
    return selected;
}

void BridgeConfig::setBluetoothConnected(bool connected) {
    portENTER_CRITICAL(&lock_);
    bool changed = btConnected_ != connected;
    btConnected_ = connected;
    portEXIT_CRITICAL(&lock_);
    if (changed) log("音频桥", "蓝牙音响%s：%s", connected ? "已连接" : "已断开",
                     selectedBtName_[0] ? selectedBtName_ : "未选择");
}

void BridgeConfig::setS3Mood(const char *mood) {
    if (!mood || !*mood) return;
    portENTER_CRITICAL(&lock_);
    strlcpy(s3Mood_, mood, sizeof(s3Mood_));
    portEXIT_CRITICAL(&lock_);
}

void BridgeConfig::setS3CommandCallback(S3CommandCallback callback) {
    s3CommandCallback_ = callback;
}

void BridgeConfig::setTfMounted(bool mounted) {
    portENTER_CRITICAL(&lock_);
    tfMounted_ = mounted;
    portEXIT_CRITICAL(&lock_);
}

void BridgeConfig::printStatus(Stream &output) {
    char configuredSsid[33];
    char btName[64];
    char btMac[18];
    char mood[48];
    bool btConnected;
    bool tfMounted;
    portENTER_CRITICAL(&lock_);
    strlcpy(configuredSsid, wifiSsid_, sizeof(configuredSsid));
    strlcpy(btName, selectedBtName_, sizeof(btName));
    strlcpy(btMac, selectedBtMac_, sizeof(btMac));
    strlcpy(mood, s3Mood_, sizeof(mood));
    btConnected = btConnected_;
    tfMounted = tfMounted_;
    portEXIT_CRITICAL(&lock_);
    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    String json = String("{\"uptime\":") + millis() +
        ",\"wifiConnected\":" + (wifiConnected ? "true" : "false") +
        ",\"wifiSsid\":\"" + jsonEscape(wifiConnected ? WiFi.SSID().c_str() : configuredSsid) + "\"" +
        ",\"staIp\":\"" + (wifiConnected ? WiFi.localIP().toString() : String()) + "\"" +
        ",\"btName\":\"" + jsonEscape(btName) + "\"" +
        ",\"btMac\":\"" + jsonEscape(btMac) + "\"" +
        ",\"btConnected\":" + (btConnected ? "true" : "false") +
        ",\"tfMounted\":" + (tfMounted ? "true" : "false") +
        ",\"s3Mood\":\"" + jsonEscape(mood) + "\"" +
        ",\"freeHeap\":" + ESP.getFreeHeap() + "}";
    output.printf("@CFG STATUS %s\n", json.c_str());
}

void BridgeConfig::scanWifi(Stream &output) {
    startWifi();
    cancelWifiOff();
    output.println("@CFG WIFI_BEGIN");
    WiFi.scanDelete();
    int count = WiFi.scanNetworks(false, true);
    /* A failed association attempt can occupy the STA scan engine and make
       scanNetworks return WIFI_SCAN_FAILED (-2).  Cancel only that pending
       attempt, keep the saved credentials, then retry once. */
    if (count < 0 && WiFi.status() != WL_CONNECTED) {
        log("音频桥", "Wi‑Fi 首次扫描失败=%d；取消当前连接尝试后重试", count);
        WiFi.disconnect(false, false);
        delay(120);
        WiFi.scanDelete();
        count = WiFi.scanNetworks(false, true);
        if (wifiSsid_[0]) connectSavedWifi();
    }
    if (count < 0) {
        output.printf("@CFG ERROR WIFI_SCAN %d\n", count);
        output.println("@CFG WIFI_END 0");
        return;
    }
    for (int i = 0; i < count; ++i) {
        String json = String("{\"ssid\":\"") + jsonEscape(WiFi.SSID(i).c_str()) +
                      "\",\"rssi\":" + WiFi.RSSI(i) +
                      ",\"secure\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
        output.printf("@CFG WIFI %s\n", json.c_str());
    }
    WiFi.scanDelete();
    requestWifiOff();
    output.printf("@CFG WIFI_END %d\n", count);
}

void BridgeConfig::printBluetoothDevices(Stream &output) {
    size_t count;
    char selectedMac[18];
    portENTER_CRITICAL(&lock_);
    count = btCount_;
    strlcpy(selectedMac, selectedBtMac_, sizeof(selectedMac));
    portEXIT_CRITICAL(&lock_);
    output.println("@CFG BT_BEGIN");
    for (size_t i = 0; i < count; ++i) {
        BtDevice device;
        portENTER_CRITICAL(&lock_);
        device = btDevices_[i];
        portEXIT_CRITICAL(&lock_);
        String json = String("{\"name\":\"") + jsonEscape(device.name) +
                      "\",\"mac\":\"" + device.mac +
                      "\",\"rssi\":" + device.rssi +
                      ",\"selected\":" + (strcasecmp(selectedMac, device.mac) == 0 ? "true" : "false") + "}";
        output.printf("@CFG BT %s\n", json.c_str());
    }
    output.printf("@CFG BT_END %u\n", static_cast<unsigned>(count));
}

bool BridgeConfig::saveWifi(const String &ssid, const String &password, String &error) {
    if (!ssid.length() || ssid.length() > 32 || password.length() > 64) {
        error = "invalid_wifi_length";
        return false;
    }
    portENTER_CRITICAL(&lock_);
    strlcpy(wifiSsid_, ssid.c_str(), sizeof(wifiSsid_));
    strlcpy(wifiPassword_, password.c_str(), sizeof(wifiPassword_));
    portEXIT_CRITICAL(&lock_);
    preferences_.putString("wifi_ssid", ssid);
    preferences_.putString("wifi_pass", password);
    startWifi();
    cancelWifiOff();
    WiFi.disconnect(false, false);
    connectSavedWifi();
    log("音频桥", "USB 工具已保存新的 Wi‑Fi：%s", ssid.c_str());
    return true;
}

bool BridgeConfig::selectBluetooth(const String &name, const String &mac, String &error) {
    if (!name.length() || name.length() > 63 || mac.length() != 17) {
        error = "invalid_bluetooth_device";
        return false;
    }
    portENTER_CRITICAL(&lock_);
    strlcpy(selectedBtName_, name.c_str(), sizeof(selectedBtName_));
    strlcpy(selectedBtMac_, mac.c_str(), sizeof(selectedBtMac_));
    portEXIT_CRITICAL(&lock_);
    preferences_.putString("bt_name", name);
    preferences_.putString("bt_mac", mac);
    log("音频桥", "USB 工具选择蓝牙音响：%s [%s]", name.c_str(), mac.c_str());
    scheduleRestart(1200);
    return true;
}

bool BridgeConfig::clearBluetoothSelection(String &error) {
    (void)error;
    preferences_.remove("bt_name");
    preferences_.remove("bt_mac");
    portENTER_CRITICAL(&lock_);
    selectedBtName_[0] = 0;
    selectedBtMac_[0] = 0;
    portEXIT_CRITICAL(&lock_);
    clearBluetoothDevices();
    log("音频桥", "USB 工具请求重新扫描蓝牙设备");
    scheduleRestart(1200);
    return true;
}

bool BridgeConfig::reconnectBluetooth(String &error) {
    if (!selectedBtMac_[0]) {
        error = "bluetooth_not_selected";
        return false;
    }
    log("音频桥", "USB 工具请求重新连接已保存的蓝牙音响");
    scheduleRestart(1200);
    return true;
}

bool BridgeConfig::updateDeepSeekKey(String key, String &error) {
    if (key.length() < 16 || key.length() > 127) {
        error = "invalid_api_key_length";
        return false;
    }
    String command = "SET_DEEPSEEK_KEY:";
    command += key;
    bool sent = s3CommandCallback_ && s3CommandCallback_(command.c_str());
    for (size_t i = 0; i < key.length(); ++i) key.setCharAt(i, '\0');
    for (size_t i = 0; i < command.length(); ++i) command.setCharAt(i, '\0');
    key = "";
    command = "";
    if (!sent) {
        error = "s3_uart_unavailable";
        return false;
    }
    log("音频桥", "已向 S3 发送 DeepSeek 密钥更新请求（内容已隐藏）");
    return true;
}

void BridgeConfig::scheduleRestart(uint32_t delayMs) {
    restartAtMs_ = millis() + delayMs;
}
