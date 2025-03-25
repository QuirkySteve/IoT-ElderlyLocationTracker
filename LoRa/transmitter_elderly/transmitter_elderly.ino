// LoRa Transmitter (TX)
#include <SPI.h>
#include <RH_RF95.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define RFM95_CS 10
#define RFM95_RST 9
#define RFM95_INT 2
#define RF95_FREQ 999.0


#define DEVICE_ID 'B'
#define ACK_TIMEOUT 1000
#define MAX_RETRIES 3


#define BROADCAST_ID 'B'  // Special ID for broadcast messages


RH_RF95 rf95(RFM95_CS, RFM95_INT);


void setupDisplay() {
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
   showMessage("Initializing TX...");


   pinMode(RFM95_RST, OUTPUT);
   digitalWrite(RFM95_RST, LOW);
   delay(20);
   digitalWrite(RFM95_RST, HIGH);
   delay(20);


   if (!rf95.init()) {
       showMessage("Setup Failed");
       delay(2000);
       while (1);
   }
   showMessage("Setup Successful");
   delay(2000);


   if (!rf95.setFrequency(RF95_FREQ)) {
       showMessage("Set Freq Failed");
       while (1);
   }


   rf95.setTxPower(13, false);
}


void sendBroadcastMessage(char msgType, const char* message) {
   uint8_t length = strlen(message);
   if (length > 24) length = 24;  // Ensure payload fits


   uint8_t packet[30]; 
   packet[0] = DEVICE_ID;  // Sender ID ('B' - User LoRa)
   packet[1] = BROADCAST_ID;  // Receiver ID ('B' - Broadcast)
   packet[2] = msgType;  // Message Type (e.g., 'P' for Ping)
   packet[3] = length + 1;  // ✅ Increase length to include NULL terminator


   memcpy(&packet[4], message, length);  // Copy message to payload
   packet[4 + length] = '\0';  // ✅ Add NULL terminator at the end


   // ✅ Compute Checksum
   uint8_t checksum = 0;
   for (int i = 0; i < 4 + length + 1; i++) { // Include NULL terminator
       checksum ^= packet[i];
   }
   packet[5 + length] = checksum;  // Store checksum


   // ✅ Debugging print
   Serial.print("Broadcasting Message: ");
   for (int i = 0; i < 6 + length; i++) {
       Serial.print(packet[i], HEX);
       Serial.print(" ");
   }
   Serial.println();


   showMessage("Broadcasting Message");
   rf95.send(packet, 6 + length); 
   rf95.waitPacketSent();


   showMessage("Broadcast Sent");
   Serial.println("Broadcast Message Sent.");
}










void loop() {
     Serial.println("🚀 Sending Broadcast...");
   sendBroadcastMessage('P', "ping");  // Broadcast ping to all LoRa shields
  
   delay(5000);  // Wait before sending another broadcast
  
}
