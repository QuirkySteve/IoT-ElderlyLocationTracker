#include <M5StickCPlus.h>  // Correct library for M5StickCPlus
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Wi-Fi Credentials
const char* ssid = "T16 6260";  
const char* password = "9{3h24U2";  

// MQTT Configuration
const char* mqtt_server = "10.132.21.209";  
const int mqtt_port = 1883;
const char* mqtt_user = "mqttuser";  
const char* mqtt_password = "yourpassword";  

// Assign a unique Anchor ID (Change for each anchor)
const int anchor_id = 5;  // Change to 2 and 3 for other anchors
String mqtt_topic = "ble/anchor/" + String(anchor_id);

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// BLE Scanner
BLEScan *pBLEScan;
int scanTime = 1;  // Reduce scan time to avoid crashes

// Target BLE Device (User)
#define TARGET_BLE_DEVICE "M5_User"

// RSSI-to-Distance Model
const float RSSI_at_1m = -60;  
const float path_loss_exponent = 2.0;

void setup() {
    M5.begin();  
    M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    Serial.begin(115200);

    M5.Lcd.setCursor(10, 10);
    M5.Lcd.printf("Anchor ID: %d", anchor_id);

    // Free unused Bluetooth memory to prevent BLE crash
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    // Delay to prevent Wi-Fi/BLE interference
    delay(1000);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Connected!");

    // Delay before MQTT connection (avoids conflicts)
    delay(2000);

    client.setServer(mqtt_server, mqtt_port);
    
    // Attempt MQTT Connection
    reconnectMQTT();

    // Initialize BLE Scanner
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);

    Serial.printf("Anchor %d initialized, publishing to: %s\n", anchor_id, mqtt_topic.c_str());
}

void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect(("M5_Anchor_" + String(anchor_id)).c_str(), mqtt_user, mqtt_password)) {
            Serial.println("Connected to MQTT Broker!");
        } else {
            Serial.println("Failed, retrying in 5 seconds...");
            delay(5000);
        }
    }
}

void loop() {
    if (!client.connected()) {
        reconnectMQTT();
    }

    BLEScanResults foundDevices = pBLEScan->start(scanTime, false);

    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        
        if (device.getName() == TARGET_BLE_DEVICE) {
            int rssi = device.getRSSI();
            float distance = pow(10, ((RSSI_at_1m - rssi) / (10 * path_loss_exponent)));

            char message[50];
            sprintf(message, "Anchor %d, Distance: %.2f meters, RSSI: %d dBm", anchor_id, distance, rssi);
            
            client.publish(mqtt_topic.c_str(), message);
            Serial.println(message);

            // Display RSSI and Distance on M5StickCPlus screen
            M5.Lcd.fillRect(0, 40, 160, 40, BLACK);  // Clear previous display
            M5.Lcd.setCursor(10, 40);
            M5.Lcd.printf("RSSI: %d dBm", rssi);
            M5.Lcd.setCursor(10, 70);
            M5.Lcd.printf("Dist: %.2f m", distance);
        }
    }

    client.loop();
    delay(500);  // Increased delay to prevent BLE overload
}
