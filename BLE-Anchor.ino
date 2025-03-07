#include <M5StickC.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Wi-Fi Credentials
const char* ssid = "WIFI-SSID";  // Replace with your Wi-Fi SSID
const char* password = "WIFI-PASSWORD"; // Replace with your Wi-Fi password

// MQTT Configuration
const char* mqtt_server = "172.20.10.2";  // Raspberry Pi IP
const int mqtt_port = 1883;
const char* mqtt_topic = "ble/distance";  // Topic for distance values
const char* mqtt_user = "mqttuser"; // MQTT Username
const char* mqtt_password = "yourpassword"; // MQTT Password

// MQTT Client Setup
WiFiClient espClient;
PubSubClient client(espClient);

// BLE Scan Setup
BLEScan *pBLEScan;
int scanTime = 3; // Scan duration in seconds

// Target BLE Device (User)
#define TARGET_BLE_DEVICE "M5_User"

// Path-loss model parameters (for distance calculation)
const float RSSI_at_1m = -60;  // Adjust this based on environment
const float path_loss_exponent = 2.0;  // Environmental factor

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
        
        // Check if the detected BLE device matches the user device
        if (device.getName() == TARGET_BLE_DEVICE) {
            int rssi = device.getRSSI();
            float distance = pow(10, ((RSSI_at_1m - rssi) / (10 * path_loss_exponent)));  // Convert RSSI to distance
            
            char message[50];
            sprintf(message, "BLE Distance: %.2f meters", distance);
            
            // Publish distance to MQTT
            client.publish(mqtt_topic, message);
            Serial.println(message);
        }
    }

    client.loop();
    delay(5000); // Wait before scanning again
}
