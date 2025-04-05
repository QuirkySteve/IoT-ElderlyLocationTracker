#include <M5StickCPlus.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// BLE name for anchors to detect
#define BLE_DEVICE_NAME "M5_User"

BLEAdvertising* pAdvertising;
unsigned long lastAdvertiseRestart = 0;

void setup() {
    M5.begin();
    Serial.begin(115200);

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    // BLE Setup
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer* pServer = BLEDevice::createServer();
    
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x12);  // Recommended for newer iOS/Android scanning
    pAdvertising->setMinInterval(250);    // 250ms
    pAdvertising->setMaxInterval(400);    // 400ms
    BLEDevice::setPower(ESP_PWR_LVL_P7);  // Max transmission power

    // Optional UUID – only needed if anchors will filter by UUID
    pAdvertising->addServiceUUID(BLEUUID((uint16_t)0x180F));

    pAdvertising->start();
    Serial.println("M5_User BLE Broadcasting Started");
}

void loop() {
    // Keep BLE alive with periodic restart every 10s
    if (millis() - lastAdvertiseRestart > 10000) {
        Serial.println("Restarting BLE Advertising...");
        pAdvertising->stop();
        delay(200);
        pAdvertising->start();
        lastAdvertiseRestart = millis();
    }

    delay(500);  // Slightly faster loop, still light
}
