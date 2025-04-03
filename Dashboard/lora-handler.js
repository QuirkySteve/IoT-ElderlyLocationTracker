const mqtt = require('mqtt');
const EventEmitter = require('events');

// Kalman Filter Class (adapted from Python)
class KalmanFilter {
    constructor(processVariance, measurementVariance, initialEstimate = 0) {
        this.processVariance = processVariance; // Q: How much the true value might change
        this.measurementVariance = measurementVariance; // R: How noisy the measurements are
        this.estimate = initialEstimate; // x_hat: Current best guess
        this.errorEstimate = 1.0; // P: How uncertain the estimate is
    }

    update(measurement) {
        // Kalman Gain: Decides how much to trust the new measurement vs the prediction
        const kalmanGain = this.errorEstimate / (this.errorEstimate + this.measurementVariance);

        // Update estimate based on measurement and gain
        this.estimate = this.estimate + kalmanGain * (measurement - this.estimate);

        // Update the error estimate (how uncertain we are now)
        // Simplified version: this.errorEstimate = (1 - kalmanGain) * this.errorEstimate;
        // Version closer to original Python example (includes process noise influence):
        this.errorEstimate = (1 - kalmanGain) * this.errorEstimate + Math.abs(this.estimate - measurement) * this.processVariance; 

        return this.estimate; // Return the filtered value
    }
}


class LoraHandler extends EventEmitter {
  constructor() {
    super();
    this.filters = {}; // Object to hold filter instances per sender ('D', 'E')
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
    if ((data.sender === 'D' || data.sender === 'E') && data.receiver === 'A' && typeof data.rssi === 'number') {
      
      const filterKey = data.sender; // 'D' or 'E'
      
      // Initialize filter if it doesn't exist for this sender
      if (!this.filters[filterKey]) {
          console.log(`Initializing Kalman Filter for sender: ${filterKey}`);
          // --- IMPORTANT: Tune these values based on observation! ---
          const processVariance = 0.05; // Example: Assume true signal doesn't change extremely rapidly
          const measurementVariance = 5;  // Example: Assume RSSI readings are moderately noisy (e.g., +/- sqrt(5) dBm)
          // --- Tune these values! ---
          this.filters[filterKey] = new KalmanFilter(processVariance, measurementVariance, data.rssi); 
      }

      // Apply the filter
      const filteredRssi = this.filters[filterKey].update(data.rssi);
      // console.log(`Sender: ${data.sender}, Raw RSSI: ${data.rssi}, Filtered RSSI: ${filteredRssi.toFixed(2)}`); // Optional debug log

      // Emit the processed message with filtered RSSI
      this.emit('lora-data', {
        type: 'lora',
        sender: data.sender, // 'D' or 'E'
        filteredRssi: filteredRssi // Use the filtered value
        // We are omitting elderlyId as it's not available in the source data
      });
    } else if (data.receiver === 'A' && typeof data.rssi !== 'number') {
        console.warn(`Received message from ${data.sender} without a valid RSSI number.`);
    }
  }
}

module.exports = LoraHandler;
