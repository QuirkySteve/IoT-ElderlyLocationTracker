#include <M5StickCPlus.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <algorithm> // Required for std::sort

// ==== CONFIGURATION ====
// Wi-Fi
const char* ssid = "HowieFold4";
const char* password = "howard1234";

// MQTT Broker
const char* mqtt_server = "192.168.139.19";
const int mqtt_port = 1883;
const char* mqtt_user = "mqttuser";
const char* mqtt_password = "yourpassword";

// Anchor ID (CHANGE THIS FOR EACH DEVICE)
const int anchor_id = 1;  // Set to 2 or 3 for other anchors

// MQTT topic
#define MQTT_TOPIC "ble/anchors"

// Target BLE device to track
#define TARGET_BLE_DEVICE "M5_User"

// BLE & RSSI Parameters
BLEScan* pBLEScan;
int scanTime = 0.2;  // 200 ms scan time

// RSSI Averaging
#define RSSI_BUFFER_SIZE 5
int rssi_buffer[RSSI_BUFFER_SIZE];
int rssi_sample_index = 0;
bool buffer_filled = false;

// Distance calculation model
const float RSSI_at_1m = -75; // Measured RSSI at 1 meter
const float path_loss_exponent = 3.0; // Adjusted for indoor/paper environment

// Timing
unsigned long lastUpdateTime = 0;

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  M5.begin();
  Serial.begin(115200);

  // Free unused Bluetooth memory
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  client.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();

  // BLE Init
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);

  // LCD Setup
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.printf("Anchor %d Ready", anchor_id);
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(("M5_Anchor_" + String(anchor_id)).c_str(), mqtt_user, mqtt_password)) {
      Serial.println("Connected.");
    } else {
      Serial.println("Failed. Retrying...");
      delay(1000);
    }
  }
}

// Function to calculate the median of the RSSI buffer
int calculateMedian(int arr[], int n) {
  // Create a temporary array to avoid modifying the original buffer
  int tempArr[n];
  for (int i = 0; i < n; i++) {
    tempArr[i] = arr[i];
  }
  // Sort the temporary array
  std::sort(tempArr, tempArr + n);
  // Return the middle element (median)
  return tempArr[n / 2];
}

float getSmoothedRSSI() {
  if (!buffer_filled) {
      // Return a default value if buffer isn't full yet
      // Using -100 as it likely results in a very large distance
      return -100.0f;
  }

  // Calculate the median of the current buffer
  int median_rssi = calculateMedian(rssi_buffer, RSSI_BUFFER_SIZE);

  return (float)median_rssi;
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }

  BLEScanResults foundDevices = pBLEScan->start(1 /* ← 1 second scan */, false);  // TEMP: more reliable

  Serial.printf("Found %d BLE devices\n", foundDevices.getCount());

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    // Print all names for debugging
    if (device.haveName()) {
      Serial.print("Device ");
      Serial.print(i);
      Serial.print(": Name = ");
      Serial.println(device.getName().c_str());
    } else {
      Serial.printf("Device %d has no name\n", i);
    }

    // Detect M5_User
    if (device.haveName() && device.getName() == TARGET_BLE_DEVICE) {
      Serial.println(">> M5_User detected <<");

      int rssi = device.getRSSI();
      rssi_buffer[rssi_sample_index] = rssi;
      rssi_sample_index = (rssi_sample_index + 1) % RSSI_BUFFER_SIZE;
      if (rssi_sample_index == 0) buffer_filled = true;

      float avg_rssi = getSmoothedRSSI();
      float distance = pow(10, ((RSSI_at_1m - avg_rssi) / (10 * path_loss_exponent)));

      char payload[80];
      snprintf(payload, sizeof(payload), "{ \"anchor\": %d, \"distance\": %.2f }", anchor_id, distance);
      client.publish(MQTT_TOPIC, payload);
      Serial.println(payload);

      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(10, 10);
      M5.Lcd.printf("Anchor %d", anchor_id);
      M5.Lcd.setCursor(10, 40);
      M5.Lcd.printf("RSSI: %d", (int)avg_rssi);
      M5.Lcd.setCursor(10, 70);
      M5.Lcd.printf("Dist: %.2fm", distance);
    }
  }

  client.loop();
  delay(200);  // short buffer between scans
}
