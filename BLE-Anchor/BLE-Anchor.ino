#include <M5StickCPlus.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <algorithm>
#include <time.h>

// ==== CONFIGURATION ====
// Wi-Fi
const char* ssid = "HowieFold4";
const char* password = "howard1234";

// MQTT
const char* mqtt_server = "192.168.139.19";
const int mqtt_port = 1883;
const char* mqtt_user = "mqttuser";
const char* mqtt_password = "yourpassword";
const int anchor_id = 3;  // Change this per anchor
#define MQTT_TOPIC "ble/anchors"

// BLE
#define TARGET_BLE_DEVICE "M5_User"
BLEScan* pBLEScan;
int scanTime = 0.2;  // 200ms

// RSSI buffer
#define RSSI_BUFFER_SIZE 5
int rssi_buffer[RSSI_BUFFER_SIZE];
int rssi_sample_index = 0;
bool buffer_filled = false;

// Distance model
const float RSSI_at_1m = -60.0;
const float path_loss_exponent = 1.43;
const float MAX_DISTANCE = 10.0f;

// MQTT
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastSeenTime = 0;

// Time (NTP)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;  // GMT+8
const int daylightOffset_sec = 0;

// ==== SETUP ====
void setup() {
  M5.begin();
  Serial.begin(115200);
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  // Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();

  // BLE
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(90);

  // NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  Serial.println("Waiting for time sync...");
  while (!getLocalTime(&timeinfo)) {
    Serial.println("⏳ Time not available yet...");
    delay(500);
  }
  Serial.println("✅ Time synced!");

  // LCD
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

int calculateMedian(int arr[], int n) {
  int tempArr[n];
  for (int i = 0; i < n; i++) tempArr[i] = arr[i];
  std::sort(tempArr, tempArr + n);
  return tempArr[n / 2];
}

float getSmoothedRSSI() {
  if (!buffer_filled) return -100.0f;
  return (float)calculateMedian(rssi_buffer, RSSI_BUFFER_SIZE);
}

// ==== MAIN LOOP ====
void loop() {
  if (!client.connected()) reconnectMQTT();

  BLEScanResults foundDevices = pBLEScan->start(1, false);  // Scan for 1 sec
  Serial.printf("Found %d BLE devices\n", foundDevices.getCount());

  bool userFound = false;

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    if (device.haveName() && device.getName() == TARGET_BLE_DEVICE) {
      userFound = true;
      lastSeenTime = millis();

      int rssi = device.getRSSI();
      rssi_buffer[rssi_sample_index] = rssi;
      rssi_sample_index = (rssi_sample_index + 1) % RSSI_BUFFER_SIZE;
      if (rssi_sample_index == 0) buffer_filled = true;

      float avg_rssi = getSmoothedRSSI();

      // ✅ Distance model with gentle fallback
      float distance;
      if (avg_rssi > -70) {
        distance = pow(10, ((RSSI_at_1m - avg_rssi) / (10 * path_loss_exponent)));
      } else {
        distance = 3.0f + 0.3f * (-avg_rssi - 70);
      }
      distance = min(distance, MAX_DISTANCE);

      Serial.printf("RSSI: %d | Smoothed: %.1f | Dist: %.2f m\n", rssi, avg_rssi, distance);

      // 📅 Get current timestamp
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      char timeStr[30];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

      // 📦 Prepare MQTT payload
      char payload[150];
      snprintf(payload, sizeof(payload),
               "{ \"anchor\": %d, \"distance\": %.2f, \"timestamp\": \"%s\" }",
               anchor_id, distance, timeStr);
      client.publish(MQTT_TOPIC, payload);
      Serial.println(payload);

      // 📺 LCD Update
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(10, 10);
      M5.Lcd.setTextColor(GREEN);
      M5.Lcd.setTextSize(2);
      M5.Lcd.printf("Anchor %d", anchor_id);
      M5.Lcd.setCursor(10, 40);
      M5.Lcd.printf("RSSI: %d", (int)avg_rssi);
      M5.Lcd.setCursor(10, 70);
      M5.Lcd.printf("Dist: %.2fm", distance);
    }
  }

  // Show fallback when user is not detected
  if (millis() - lastSeenTime > 3000) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("No M5_User");
    M5.Lcd.setCursor(10, 40);
    M5.Lcd.printf("Anchor %d", anchor_id);
  }

  client.loop();
  delay(200);
}
