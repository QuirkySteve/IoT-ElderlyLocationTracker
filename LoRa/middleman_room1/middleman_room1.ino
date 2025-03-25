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
#define RF95_FREQ 999.0  // Adjust frequency based on region


#define DEVICE_ID 'D'
#define DESTINATION_ID 'A' // Processing node


#define BROADCAST_ID 'B'  // Special ID for broadcast messages


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
       uint8_t buf[32];
       uint8_t len = sizeof(buf);


       if (rf95.recv(buf, &len)) {
           uint8_t senderID = buf[0]; 
           uint8_t receiverID = buf[1]; 
           char msgType = buf[2]; 
           uint8_t msgLength = buf[3];


           // Measure RSSI of the received message from 'B'
           int16_t rssi = rf95.lastRssi();


           // ✅ Debugging: Print received packet
           Serial.print("Received Packet at ");
           Serial.print((char)DEVICE_ID);
           Serial.print(": ");
           for (int i = 0; i < len; i++) {
               Serial.print(buf[i], HEX);
               Serial.print(" ");
           }
           Serial.println();


           Serial.print("Measured RSSI at ");
           Serial.print((char)DEVICE_ID);
           Serial.print(": ");
           Serial.println(rssi);


           // ✅ Accept messages meant for this device OR broadcasted
           if (receiverID == DEVICE_ID || receiverID == BROADCAST_ID) {
               showMessage("Broadcast Received");
               Serial.print("Received Broadcast at ");
               Serial.print((char)DEVICE_ID);
               Serial.print(": ");
               Serial.println((char*)&buf[4]); // Print the actual message


               // ✅ Send ACK back to sender
               sendAck(senderID);


               // ✅ Delay slightly before forwarding to avoid collision
               delay(random(50, 200));  // Random delay between 50-200ms


               // ✅ Forward message to `A` with RSSI
               forwardMessageToA(senderID, buf, msgLength, rssi);
           }
       }
   }
}




// Function to send ACK response
void sendAck(uint8_t senderID) {
   uint8_t ack[5]; 
   ack[0] = DEVICE_ID;   
   ack[1] = senderID;    
   ack[2] = 'A';         
   ack[3] = 1;           
   ack[4] = ack[0] ^ ack[1] ^ ack[2] ^ ack[3]; 


   rf95.send(ack, sizeof(ack));
   rf95.waitPacketSent();


   showMessage("ACK Sent");
   Serial.println("ACK Packet Sent.");
}


// Function to send JSON ACK to Device B
void sendJsonAck(uint8_t senderID) {
   uint8_t ack[5]; 
   ack[0] = DEVICE_ID;    // Acknowledging device
   ack[1] = senderID;     // Sender ID (B)
   ack[2] = 'A';          // ACK type
   ack[3] = 1;            // Payload Length
   ack[4] = ack[0] ^ ack[1] ^ ack[2] ^ ack[3];  // Checksum


   rf95.send(ack, sizeof(ack));
   rf95.waitPacketSent();


   showMessage("ACK Sent");
   Serial.println("ACK Packet Sent.");
}


void forwardMessageToA(uint8_t senderID, uint8_t* originalMsg, uint8_t msgLength, int16_t rssi) {
   uint8_t newMsg[32];

   newMsg[0] = DEVICE_ID;       // ✅ Ensure sender is 'D' or 'E'
   newMsg[1] = 'A';             // ✅ Forwarding to Processing Shield 'A'
   newMsg[2] = 'M';             // ✅ Message type remains 'M'
   newMsg[3] = msgLength + 2;   // ✅ Adjust for RSSI (2 extra bytes)

   // Copy message payload including null terminator
   memcpy(&newMsg[4], originalMsg + 4, msgLength);  // Copy entire message including null

   // ✅ Append RSSI correctly (Little Endian)
   newMsg[4 + msgLength] = (uint8_t)(rssi & 0xFF);        // Lower byte
   newMsg[4 + msgLength + 1] = (uint8_t)((rssi >> 8) & 0xFF); // Upper byte

   // ✅ Compute new checksum before sending
   uint8_t checksum = 0;
   for (int i = 0; i < 4 + msgLength + 2; i++) { 
       checksum ^= newMsg[i];
   }
   newMsg[4 + msgLength + 2] = checksum;  // ✅ Append checksum

   rf95.send(newMsg, 7 + msgLength);
   rf95.waitPacketSent();
}