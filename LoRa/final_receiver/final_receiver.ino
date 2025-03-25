#include <SPI.h>
#include <RH_RF95.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(64, 32, &Wire, -1);


#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 999.0  // Adjust frequency as needed


#define DEVICE_ID 'A'  // This is Processing Node A


RH_RF95 rf95(RFM95_CS, RFM95_INT);


void setupDisplay() {
   Wire.begin();
   delay(100);


   if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
       Serial.println(F("SSD1306 allocation failed"));
       while (1);
   }


   display.setTextSize(1);
   display.setTextColor(WHITE);
   display.clearDisplay();
   display.display();
}


void showMessage(const char* message) {
   display.clearDisplay();
   display.setCursor(0, 0);
   display.println(message);
   display.display();
   Serial.println(message);
}


void setup() {
   Serial.begin(9600);
   delay(100);


   setupDisplay();
   showMessage("Initializing RX...");


   pinMode(RFM95_RST, OUTPUT);
   digitalWrite(RFM95_RST, LOW);
   delay(10);
   digitalWrite(RFM95_RST, HIGH);
   delay(10);


   if (!rf95.init()) {
       showMessage("Setup Failed");
       while (1);
   }
   showMessage("Setup Successful");


   if (!rf95.setFrequency(RF95_FREQ)) {
       showMessage("Set Freq Failed");
       while (1);
   }


   rf95.setTxPower(13, false);
}


void loop() {
   if (rf95.available()) {
       uint8_t buf[40];
       uint8_t len = sizeof(buf);


       if (rf95.recv(buf, &len)) {
           uint8_t senderID = buf[0];  // 'D' or 'E'
           uint8_t receiverID = buf[1]; // 'A'
           char msgType = buf[2]; 
           uint8_t msgLength = buf[3] - 2;  // Adjust for RSSI bytes


           // ✅ Extract RSSI Properly (Little Endian)
           int16_t rssi = (int16_t)((buf[4 + msgLength + 1] << 8) | buf[4 + msgLength]);


           // ✅ Validate Checksum
           uint8_t checksum = 0;
           for (int i = 0; i < 4 + msgLength + 2; i++) { 
               checksum ^= buf[i];
           }


           if (checksum != buf[4 + msgLength + 2]) {
               Serial.println("{ \"error\": \"Checksum mismatch, ignoring message\" }");
               return;
           }


           // ✅ Create JSON object
           StaticJsonDocument<256> doc;
           doc["sender"] = String((char)senderID);
           doc["receiver"] = String((char)receiverID);
           doc["type"] = String(msgType);
           doc["message"] = String((char*)&buf[4]);
           doc["rssi"] = rssi;


           // ✅ Serialize JSON to Serial
           char jsonBuffer[256];
           serializeJson(doc, jsonBuffer);
           Serial.println(jsonBuffer);  // Print JSON formatted message
       }
   }
}


// Function to send ACK back to `D` or `E`
void sendAck(uint8_t senderID) {
   uint8_t ack[5]; 
   ack[0] = DEVICE_ID;    // Receiver ID ('A')
   ack[1] = senderID;     // Sender ID ('D' or 'E')
   ack[2] = 'A';          // ACK type
   ack[3] = 1;            // Payload Length
   ack[4] = ack[0] ^ ack[1] ^ ack[2] ^ ack[3];  // Checksum


   rf95.send(ack, sizeof(ack));
   rf95.waitPacketSent();


   showMessage("✅ ACK Sent");
   Serial.println("✅ ACK Packet Sent.");
}
