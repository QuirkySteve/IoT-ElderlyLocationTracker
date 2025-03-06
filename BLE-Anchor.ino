#include <M5StickC.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Wi-Fi Credentials
const char* ssid = "QuirkySteve";  // Replace with your Wi-Fi SSID
const char* password = "93980728"; // Replace with your Wi-Fi password

// MQTT Configuration
const char* mqtt_server = "172.20.10.2";  // Raspberry Pi IP
const int mqtt_port = 1883;
const char* mqtt_user = "mqttuser";  // MQTT Username
const char* mqtt_password = "yourpassword";  // MQTT Password
const char* mqtt_topic = "ble/anchor";  // Topic for RSSI values

// MQTT Client Setup
WiFiClient espClient;
PubSubClient client(espClient);

// BLE Scan Setup
BLEScan *pBLEScan;
int scanTime = 5; // Scan duration in seconds

// Device to track
#define TARGET_BLE_DEVICE "M5_User"

void setup() {
    M5.begin();
    Serial.begin(115200);
    
    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Connected!");

    // Setup MQTT
    client.setServer(mqtt_server, mqtt_port);
    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("M5_Anchor", mqtt_user, mqtt_password)) {
            Serial.println("Connected to MQTT Broker!");
        } else {
            Serial.println("Failed. Retrying in 5 seconds...");
            delay(5000);
        }
    }

    // Initialize BLE Scanner
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);  // Active scan for better RSSI accuracy
    Serial.println("BLE Scanner Initialized...");
}

void loop() {
    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
    
    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        
        // Check if the detected BLE device matches the target user
        if (device.getName() == TARGET_BLE_DEVICE) {
            int rssi = device.getRSSI();
            char message[50];
            sprintf(message, "Anchor RSSI: %d dBm", rssi);
            
            // Publish RSSI data to MQTT
            client.publish(mqtt_topic, message);
            Serial.println(message);
        }
    }

    client.loop();
    delay(5000); // Wait before scanning again
}
