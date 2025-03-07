# **IoT Elderly Location Tracking System Setup Guide**

## **Overview**
This project implements a location tracking system using **M5StickCPlus devices**, **BLE communication**, and **MQTT messaging** to enable distance estimation and triangulation calculations on a **Raspberry Pi 4 MQTT broker**.

The system consists of:
- **M5StickCPlus User Device**: Broadcasts BLE signals.
- **M5StickCPlus Anchors**: Scan BLE signals, estimate distance using RSSI, and send the data to MQTT.
- **Raspberry Pi 4 (MQTT Broker)**: Manages MQTT messaging, processes distance values, and performs triangulation.
- **LoRa Gateway (WizGate - In Progress)**: Being set up to support LoRa-based tracking.
- **LilyGo T3S3 LoRa Anchors (In Progress)**: To extend tracking coverage between spaces.
- **M5-User Device with LoRa Pairing (In Progress)**: A LoRa module is being paired with the M5-User device for both **precise location tracking within a space (via BLE)** and **general location tracking between spaces (via LoRa anchors)**.

---

## **System Components & Their Functions**

### **1. Raspberry Pi 4 (MQTT Broker)**
- Runs **Mosquitto MQTT Broker** to manage communications between M5StickCPlus devices.
- Receives **BLE distance measurements** from M5StickCPlus anchors.
- Computes **position triangulation** using RSSI-based distance estimation.

### **2. M5StickCPlus User Device (BLE Broadcaster)**
- Broadcasts a **BLE advertisement signal** at regular intervals.
- Detected by **M5StickCPlus Anchors** to measure signal strength (RSSI).
- (Upcoming) **Will be paired with a LoRa module** for extended tracking across larger areas.

### **3. M5StickCPlus Anchors (BLE Receivers)**
- Scan for BLE advertisements from the **User Device**.
- Measure **RSSI values** and estimate **distance** using the path-loss model.
- Publish distance values to the **MQTT broker**.

### **4. LoRa Gateway - WizGate (Upcoming)**
- A LoRa-based gateway is currently being set up to manage **LoRa anchors** and relay location data over MQTT.

### **5. LilyGo T3S3 LoRa Anchors (Upcoming)**
- Being configured to extend **location tracking between spaces** where BLE coverage is limited.
- LoRa anchors will communicate with the WizGate LoRa gateway to improve tracking range.

---

## **Dependencies & Installation**

### **1. Setting Up MQTT Broker on Raspberry Pi 4**

#### **Step 1: Update & Install Mosquitto MQTT Broker**
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y mosquitto mosquitto-clients
```

#### **Step 2: Enable & Start Mosquitto**
```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

#### **Step 3: Allow Remote Connections (Modify Mosquitto Configuration)**
1. Open the Mosquitto config file:
   ```bash
   sudo nano /etc/mosquitto/mosquitto.conf
   ```
2. Add the following lines:
   ```
   listener 1883
   allow_anonymous true
   ```
3. Save and exit (`CTRL + X`, then `Y`, then `Enter`).
4. Restart Mosquitto:
   ```bash
   sudo systemctl restart mosquitto
   ```

#### **Step 4: Verify MQTT Broker is Running**
To check if Mosquitto is running:
```bash
sudo systemctl status mosquitto
```
To test MQTT publishing and subscribing:
1. Open a terminal and subscribe:
   ```bash
   mosquitto_sub -h localhost -t "test/topic"
   ```
2. Open another terminal and publish a message:
   ```bash
   mosquitto_pub -h localhost -t "test/topic" -m "MQTT is working!"
   ```
If you see the message appear, Mosquitto is running correctly.

#### **Step 5: Subscribing to M5-Anchor MQTT Topics on Raspberry Pi**
To subscribe to MQTT messages from the **M5StickCPlus Anchors**, run:
```bash
mosquitto_sub -h 'your-broker-ipaddress' -t "ble/distance" -u mqttuser -P yourpassword
```
Replace `your-broker-ipaddress` with your Raspberry Pi’s IP address.

---

### **2. Installing Dependencies on M5StickCPlus (Arduino IDE)**
To program the **M5StickCPlus devices**, install the following libraries:
1. Open **Arduino IDE**.
2. Go to **Sketch → Include Library → Manage Libraries**.
3. Search and install:
   - **M5StickC** (by M5Stack)
   - **ESP32 BLE Arduino** (by Neil Kolban)
   - **PubSubClient** (by Nick O’Leary)

#### **Board & Port Setup in Arduino IDE**
1. Go to **Tools → Board → ESP32 Arduino → M5StickC**.
2. Select the correct **COM port**.
3. Compile and upload the correct script to each M5StickCPlus device.

---

## **Configuring Devices for Different Networks**
If setting up the system on a **different Wi-Fi network**, modify the following variables in the **M5StickCPlus code** before uploading:

```cpp
// Wi-Fi Credentials
const char* ssid = "YourNewSSID";  // Replace with the new Wi-Fi SSID
const char* password = "YourNewPassword";  // Replace with the new Wi-Fi password

// MQTT Configuration
const char* mqtt_server = "your-broker-ipaddress";  // Replace with the new Raspberry Pi IP Address
```
Ensure the new Raspberry Pi IP is correct by running:
```bash
hostname -I
```

---



