#include <M5StickC.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Define BLE Device Name
#define BLE_DEVICE_NAME "M5_User"

void setup() {
    M5.begin();
    Serial.begin(115200);

    // Initialize BLE
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEServer *pServer = BLEDevice::createServer();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLEUUID((uint16_t)0x180F));  // Random UUID
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    
    BLEDevice::startAdvertising();
    Serial.println("BLE Broadcasting Started...");
}

void loop() {
    // Keep BLE running
    delay(1000);
}
