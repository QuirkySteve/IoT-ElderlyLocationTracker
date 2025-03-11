#include <M5StickCPlus.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Define BLE Device Name
#define BLE_DEVICE_NAME "M5_User"

// BLE Advertising Object
BLEAdvertising *pAdvertising;

void setup() {
    M5.begin();
    Serial.begin(115200);

    // Free unused Bluetooth memory to prevent crashes
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    // Initialize BLE
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer *pServer = BLEDevice::createServer();
    
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLEUUID((uint16_t)0x180F));  // Random UUID
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);

    // Set the BLE power level for better range (adjustable: P0 to P7)
    BLEDevice::setPower(ESP_PWR_LVL_P7);  // P7 = Max Power

    // Configure Advertising Interval (Lower = More Frequent, Higher = Saves Power)
    pAdvertising->setMinInterval(400);  // Default is 160 (250ms), increased for stability
    pAdvertising->setMaxInterval(600);  // Default is 200 (312.5ms), increased for stability

    // Start BLE Advertising
    pAdvertising->start();
    Serial.println("BLE Broadcasting Started...");
}

void loop() {
    // Check if BLE advertising is active and restart if needed
    if (!BLEDevice::getAdvertising()) {
        Serial.println("BLE Advertising Stopped, Restarting...");
        pAdvertising->start();
    }

    delay(1000);  // Keep BLE running
}
