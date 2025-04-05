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

    // BLE MUST be initialized before querying MAC
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer* pServer = BLEDevice::createServer();

    // Get MAC address AFTER BLE is initialized
    String mac = BLEDevice::getAddress().toString().c_str();

    // LCD Display
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("🟦 USER DEVICE");
    M5.Lcd.print("Name: ");
    M5.Lcd.println(BLE_DEVICE_NAME);
    M5.Lcd.print("MAC:\n");
    M5.Lcd.println(mac);

    // BLE Advertising Setup
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x12);
    pAdvertising->setMinInterval(250);
    pAdvertising->setMaxInterval(400);
    BLEDevice::setPower(ESP_PWR_LVL_P7);
    pAdvertising->addServiceUUID(BLEUUID((uint16_t)0x180F));
    pAdvertising->start();

    Serial.println("M5_User BLE Broadcasting Started");
}

void loop() {
    // Keep BLE alive with periodic restart
    if (millis() - lastAdvertiseRestart > 10000) {
        Serial.println("Restarting BLE Advertising...");
        pAdvertising->stop();
        delay(200);
        pAdvertising->start();
        lastAdvertiseRestart = millis();
    }

    delay(500);
}
