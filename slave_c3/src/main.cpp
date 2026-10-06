#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "freertos/queue.h"

#include "config.h"
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <ir_Gree.h>

static constexpr uint32_t CodeMagic = 0x43415449; // "CATI"
static constexpr uint16_t CodeVersion = 2;
static constexpr size_t MaxRawEntries = RAW_BUFFER_LENGTH - 1;
static constexpr uint8_t AcTemperature = 26;

struct StoredIrCode {
    uint32_t magic;
    uint16_t version;
    uint16_t rawLength;
    uint16_t carrierKhz;
    uint16_t raw[MaxRawEntries];
};

struct RxMessage {
    uint8_t sender[6];
    char text[48];
};

static Preferences Storage;
static QueueHandle_t CommandQueue;
static IRsend RawSender(IR_TX_GPIO);
static IRrecv Receiver(IR_RX_GPIO, RAW_BUFFER_LENGTH, 50, true);
static decode_results DecodeResult;
/* Test the library's native GREE protocol with its default YAW1F model. */
static IRGreeAC Aircon(IR_TX_GPIO);

static bool validName(const char *name) {
    return strcmp(name, "AC_ON") == 0 || strcmp(name, "AC_OFF") == 0 ||
           strcmp(name, "TV_ON") == 0 || strcmp(name, "TV_OFF") == 0;
}

static void sendReply(const uint8_t peer[6], const String &text) {
    if (!esp_now_is_peer_exist(peer)) {
        esp_now_peer_info_t info{};
        memcpy(info.peer_addr, peer, 6);
        info.ifidx = WIFI_IF_STA;
        info.channel = ESPNOW_WIFI_CHANNEL;
        info.encrypt = false;
        esp_now_add_peer(&info);
    }
    esp_now_send(peer, reinterpret_cast<const uint8_t *>(text.c_str()), text.length() + 1);
}

static bool loadCode(const char *name, StoredIrCode &code) {
    size_t stored = Storage.getBytesLength(name);
    if (stored != sizeof(code)) return false;
    if (Storage.getBytes(name, &code, sizeof(code)) != sizeof(code)) return false;
    return code.magic == CodeMagic && code.version == CodeVersion &&
           code.rawLength > 2 && code.rawLength <= MaxRawEntries;
}

static bool sendAircon(bool power) {
    Receiver.disableIRIn();
    Aircon.setMode(kGreeCool);
    Aircon.setTemp(AcTemperature);
    Aircon.setFan(kGreeFanAuto);
    Aircon.setSwingVertical(true, kGreeSwingAuto);
    Aircon.setSwingHorizontal(kGreeSwingHOff);
    Aircon.setTurbo(false);
    Aircon.setLight(true);
    Aircon.setPower(power);
    Aircon.send();
    Receiver.enableIRIn();
    Serial.printf("Sent GREE/default-YAW1F AC power=%s mode=cool temp=%u fan=auto\n",
                  power ? "on" : "off", AcTemperature);
    return true;
}

static bool transmit(const char *name) {
    if (strcmp(name, "AC_ON") == 0) return sendAircon(true);
    if (strcmp(name, "AC_OFF") == 0) return sendAircon(false);
    StoredIrCode code{};
    if (!loadCode(name, code)) return false;
    Receiver.disableIRIn();
    RawSender.sendRaw(code.raw, code.rawLength, code.carrierKhz);
    Receiver.enableIRIn();
    Serial.printf("Sent learned %s (%u raw timings)\n", name, code.rawLength);
    return true;
}

static bool learnCode(const char *name) {
    Serial.printf("Learning %s: press the original remote within %u seconds\n",
                  name, LEARN_TIMEOUT_MS / 1000);
    uint32_t started = millis();
    while (millis() - started < LEARN_TIMEOUT_MS) {
        if (!Receiver.decode(&DecodeResult)) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
        if (DecodeResult.overflow || DecodeResult.rawlen < 4) {
            Serial.printf("IR frame ignored: rawlen=%u overflow=%u\n",
                          DecodeResult.rawlen, DecodeResult.overflow);
            Receiver.resume();
            continue;
        }
        size_t length = DecodeResult.rawlen - 1;
        if (length > MaxRawEntries) { Receiver.resume(); return false; }
        StoredIrCode code{};
        code.magic = CodeMagic;
        code.version = CodeVersion;
        code.rawLength = length;
        code.carrierKhz = IR_CARRIER_KHZ;
        for (size_t i = 1; i < DecodeResult.rawlen; ++i) {
            uint32_t usec = DecodeResult.rawbuf[i] * kRawTick;
            code.raw[i - 1] = usec > UINT16_MAX ? UINT16_MAX : usec;
        }
        Receiver.resume();
        bool saved = Storage.putBytes(name, &code, sizeof(code)) == sizeof(code);
        Serial.printf("%s %s (%u entries, decoded=%s)\n", name,
                      saved ? "saved" : "save failed", code.rawLength,
                      typeToString(DecodeResult.decode_type).c_str());
        return saved;
    }
    Serial.printf("Learning %s timed out\n", name);
    return false;
}

static void testReceiverPin() {
    Serial.printf("IR_RX_TEST started on GPIO%u: press remote now (10 seconds)\n", IR_RX_GPIO);
    uint32_t started = millis(), transitions = 0, lowSamples = 0;
    int previous = digitalRead(IR_RX_GPIO);
    while (millis() - started < 10000) {
        int current = digitalRead(IR_RX_GPIO);
        if (current != previous) { ++transitions; previous = current; }
        if (current == LOW) ++lowSamples;
        delayMicroseconds(50);
    }
    Serial.printf("IR_RX_TEST result transitions=%u low_samples=%u idle_level=%d\n",
                  transitions, lowSamples, digitalRead(IR_RX_GPIO));
}

static void onDataReceived(const uint8_t *mac, const uint8_t *data, int length) {
    if (!mac || !data || length <= 0) return;
    RxMessage message{};
    memcpy(message.sender, mac, 6);
    size_t copy = min(static_cast<size_t>(length), sizeof(message.text) - 1);
    memcpy(message.text, data, copy);
    message.text[copy] = 0;
    xQueueSend(CommandQueue, &message, 0);
}

static void commandTask(void *) {
    RxMessage message;
    for (;;) {
        if (xQueueReceive(CommandQueue, &message, portMAX_DELAY) != pdTRUE) continue;
        Serial.printf("ESP-NOW command: %s\n", message.text);
        if (strncmp(message.text, "LEARN:", 6) == 0 && validName(message.text + 6)) {
            bool ok = learnCode(message.text + 6);
            sendReply(message.sender, ok ? String("OK:LEARNED:") + (message.text + 6) : "ERR:LEARN_FAILED");
        } else if (validName(message.text)) {
            bool ok = transmit(message.text);
            sendReply(message.sender, ok ? String("OK:SENT:") + message.text : "ERR:CODE_NOT_LEARNED");
        } else if (strcmp(message.text, "PING") == 0) {
            sendReply(message.sender, "OK:PONG");
        } else {
            sendReply(message.sender, "ERR:UNKNOWN_COMMAND");
        }
    }
}

static void printMac() {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    Serial.printf("C3 STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void setup() {
    Serial.begin(115200);
    Storage.begin("ir-codes", false);
    RawSender.begin();
    Aircon.begin();
    Receiver.enableIRIn();
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    printMac();
    CommandQueue = xQueueCreate(8, sizeof(RxMessage));
    if (!CommandQueue || esp_now_init() != ESP_OK) abort();
    esp_now_register_recv_cb(onDataReceived);
    xTaskCreate(commandTask, "esp_now_ir_task", 6144, nullptr, 5, nullptr);
    Serial.printf("Cyber Cat C3 ready on channel %u. AC=GREE/default-YAW1F, IR TX=GPIO%u\n",
                  ESPNOW_WIFI_CHANNEL, IR_TX_GPIO);
}

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "PRINT_MAC") printMac();
        else if (command == "TEST_RX") testReceiverPin();
        else if (command.startsWith("LEARN:") && validName(command.c_str() + 6)) learnCode(command.c_str() + 6);
        else if (validName(command.c_str())) transmit(command.c_str());
    }
    delay(10);
}
