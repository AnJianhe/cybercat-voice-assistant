#pragma once

#include "config.h"
#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>

class BridgeConfig {
public:
    using S3CommandCallback = bool (*)(const char *command);

    void begin();
    void startWifi();
    void stopWifi();
    void requestWifiOff(uint32_t delayMs = 60000);
    void cancelWifiOff();
    bool beginStorage();
    void loop();
    bool ensureWifi(uint32_t timeoutMs);

    bool onBluetoothDevice(const char *name, const uint8_t address[6], int rssi);
    void setBluetoothConnected(bool connected);
    void setS3Mood(const char *mood);
    void setTfMounted(bool mounted);
    void setS3CommandCallback(S3CommandCallback callback);

    void log(const char *source, const char *format, ...);
    void dumpPersistentLog(Stream &output);
    void printPersistentLogStatus(Stream &output);
    bool clearPersistentLog();

    void printStatus(Stream &output);
    void scanWifi(Stream &output);
    void printBluetoothDevices(Stream &output);
    bool saveWifi(const String &ssid, const String &password, String &error);
    bool selectBluetooth(const String &name, const String &mac, String &error);
    bool clearBluetoothSelection(String &error);
    bool reconnectBluetooth(String &error);
    bool updateDeepSeekKey(String key, String &error);
    void scheduleRestart(uint32_t delayMs = 1000);

private:
    struct BtDevice {
        char name[64];
        char mac[18];
        int rssi;
        uint32_t seenMs;
    };
    Preferences preferences_;
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    BtDevice btDevices_[BRIDGE_MAX_BT_DEVICES]{};
    size_t btCount_ = 0;
    char wifiSsid_[33]{};
    char wifiPassword_[65]{};
    char selectedBtName_[64]{};
    char selectedBtMac_[18]{};
    char s3Mood_[48]{"unknown"};
    S3CommandCallback s3CommandCallback_ = nullptr;
    bool btConnected_ = false;
    bool tfMounted_ = false;
    bool storageReady_ = false;
    SemaphoreHandle_t storageLock_ = nullptr;
    bool lastWifiConnected_ = false;
    volatile bool wifiStarted_ = false;
    uint32_t lastWifiAttemptMs_ = 0;
    volatile uint32_t wifiOffAtMs_ = 0;
    volatile uint32_t restartAtMs_ = 0;

    void connectSavedWifi();
    void clearBluetoothDevices();
    void appendPersistentLog(uint32_t ms, const char *source, const char *text);
    static String jsonEscape(const char *value);
};

extern BridgeConfig DeviceConfig;
