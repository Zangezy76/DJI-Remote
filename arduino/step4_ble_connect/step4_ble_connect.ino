#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <NimBLEDevice.h>

// ─── Камера ───────────────────────────────────────────────
static const char* CAMERA_MAC = "04:a8:5a:8c:bc:6c";

// ─── UI ───────────────────────────────────────────────────
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* logLabel    = nullptr;
static char statusText[128]  = "Starting...";
static char logText[1024]    = "";
static bool statusChanged    = false;
static bool logChanged       = false;

// ─── BLE ──────────────────────────────────────────────────
static NimBLEClient* pClient  = nullptr;
static bool          connected = false;
static bool          triggerCmd = false;  // PWR кнопка

// ─── Callbacks ────────────────────────────────────────────
class CameraCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient* c, int reason) override {
        connected = false;
        snprintf(statusText, sizeof(statusText), "DISCONNECTED\nreason: %d", reason);
        statusChanged = true;
        Serial.printf("[BLE] Disconnected, reason=%d\n", reason);
    }
};

// ─── Discovery ────────────────────────────────────────────
void discoverServices() {
    logText[0] = '\0';
    char tmp[128];

    auto services = pClient->getServices(true);
    if (!services || services->empty()) {
        strlcat(logText, "No services!\n", sizeof(logText));
        logChanged = true;
        return;
    }

    for (auto& svc : *services) {
        snprintf(tmp, sizeof(tmp), "SVC %s\n", svc->getUUID().toString().c_str());
        strlcat(logText, tmp, sizeof(logText));
        Serial.printf("[SVC] %s\n", svc->getUUID().toString().c_str());

        auto chars = svc->getCharacteristics(true);
        for (auto& chr : *chars) {
            snprintf(tmp, sizeof(tmp), "  CHR %s [%s]\n",
                chr->getUUID().toString().c_str(),
                chr->propertiesStr().c_str());
            strlcat(logText, tmp, sizeof(logText));
            Serial.printf("  [CHR] %s | %s\n",
                chr->getUUID().toString().c_str(),
                chr->propertiesStr().c_str());

            // Подписаться на все NOTIFY — смотрим что шлёт камера
            if (chr->canNotify()) {
                chr->subscribe(true, [](NimBLERemoteCharacteristic* c,
                                        uint8_t* data, size_t len, bool isNotify) {
                    Serial.printf("  [NOTIFY] %s →", c->getUUID().toString().c_str());
                    for (size_t i = 0; i < len; i++) Serial.printf(" %02X", data[i]);
                    Serial.println();
                });
            }
        }
    }
    logChanged = true;
}

// ─── BLE Task ─────────────────────────────────────────────
void bleTask(void* pvParameters) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    NimBLEDevice::init("TWatch-DJI");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(512);

    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new CameraCallbacks(), false);
    pClient->setConnectTimeout(10);

    NimBLEAddress addr(std::string(CAMERA_MAC), BLE_ADDR_PUBLIC);

    snprintf(statusText, sizeof(statusText), "Connecting...\n%s", CAMERA_MAC);
    statusChanged = true;

    Serial.println("[BLE] Connecting...");

    if (!pClient->connect(addr)) {
        snprintf(statusText, sizeof(statusText), "FAILED!\nCamera off?");
        statusChanged = true;
        Serial.println("[BLE] Connection failed!");
        vTaskDelete(NULL);
        return;
    }

    connected = true;
    snprintf(statusText, sizeof(statusText), "Connected!\nDiscovering...");
    statusChanged = true;
    Serial.println("[BLE] Connected! Discovering services...");

    vTaskDelay(300 / portTICK_PERIOD_MS);
    discoverServices();

    snprintf(statusText, sizeof(statusText), "Connected!\nRSSI: %d dBm", pClient->getRssi());
    statusChanged = true;

    // Держим соединение + обновляем RSSI каждые 3 сек
    while (connected) {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        if (connected) {
            snprintf(statusText, sizeof(statusText), "Connected!\nRSSI: %d dBm", pClient->getRssi());
            statusChanged = true;
        }
    }

    vTaskDelete(NULL);
}

// ─── PWR кнопка ───────────────────────────────────────────
void onEvent(DeviceEvent_t event, void* params, void* user_data) {
    if (event == POWER_EVENT) {
        auto pmu = instance.getPMUEventType(params);
        if (pmu == PMU_EVENT_KEY_CLICKED) {
            Serial.println("[BTN] PWR clicked");
            instance.setHapticEffects(14);
            instance.vibrator();
            triggerCmd = true;
        }
    }
}

// ─── Setup ────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    instance.begin();
    beginLvglHelper(instance);
    instance.setBrightness(200);
    instance.onEvent(onEvent, NULL, ALL_EVENT_MAX);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    // Статус вверху
    statusLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(statusLabel, statusText);
    lv_obj_set_style_text_color(statusLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_width(statusLabel, 390);
    lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

    // Лог сервисов внизу
    logLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(logLabel, "");
    lv_obj_set_style_text_color(logLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_font(logLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_width(logLabel, 400);
    lv_label_set_long_mode(logLabel, LV_LABEL_LONG_WRAP);
    lv_obj_align(logLabel, LV_ALIGN_TOP_LEFT, 5, 100);

    xTaskCreate(bleTask, "ble", 8192, NULL, 1, NULL);
}

// ─── Loop ─────────────────────────────────────────────────
void loop() {
    instance.loop();

    if (statusChanged) {
        statusChanged = false;
        lv_label_set_text(statusLabel, statusText);
    }
    if (logChanged) {
        logChanged = false;
        lv_label_set_text(logLabel, logText);
    }

    // PWR кнопка — команды добавим после discovery
    if (triggerCmd) {
        triggerCmd = false;
        if (connected) {
            Serial.println("[CMD] Ready — paste characteristic UUID to send command");
        } else {
            Serial.println("[CMD] Not connected!");
        }
    }

    lv_task_handler();
    delay(5);
}
