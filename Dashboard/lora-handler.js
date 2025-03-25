const mqtt = require('mqtt');
const EventEmitter = require('events');

class LoraHandler extends EventEmitter {
  constructor() {
    super();
    this.client = mqtt.connect({
      host: '192.168.139.19',
      port: 1883,
      username: 'mqttuser',
      password: 'yourpassword'
    });

    this.client.on('connect', () => {
      this.client.subscribe('lora/received');
      console.log('LoRa MQTT connected');
    });

    this.client.on('message', (topic, message) => {
      if (topic === 'lora/received') {
        this.processMessage(JSON.parse(message.toString()));
      }
    });
  }

  processMessage(data) {
    // Only process messages from D->A or E->A
    if ((data.sender === 'D' || data.sender === 'E') && data.receiver === 'A') {
      // Emit the processed message for the server to handle
      this.emit('lora-data', {
        type: 'lora',
        sender: data.sender,
        rssi: data.rssi
      });
    }
  }
}

module.exports = LoraHandler;