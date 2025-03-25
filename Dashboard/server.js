// Add at top
const LoraHandler = require('./lora-handler');

// Initialize after BLE setup
const loraHandler = new LoraHandler();

// Install dependencies: npm install express ws mqtt
const express = require('express');
const WebSocket = require('ws');
const mqtt = require('mqtt');

const app = express();
const server = require('http').createServer(app);
const wss = new WebSocket.Server({ server });

const MQTT_BROKER = 'mqtt://192.168.183.19'; // Note the double slashes
const BLE_TOPIC = 'ble/anchors'; // Topic for BLE data

const mqttClient = mqtt.connect(MQTT_BROKER);

// Store latest distances from anchors
const anchorData = {};

mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
    mqttClient.subscribe(BLE_TOPIC);
});

// Handle LoRa messages
loraHandler.on('lora-data', (data) => {
    console.log('Received LoRa data:', data);
    // Send to all connected WebSocket clients
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify(data));
        }
    });
});

mqttClient.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        
        if (topic === BLE_TOPIC) {
            const anchorId = parseInt(data.anchor);
            const distance = parseFloat(data.distance);

            anchorData[anchorId] = distance;

            // Proceed only when all 3 anchors have reported
            if (anchorData[1] && anchorData[2] && anchorData[3]) {
                const position = trilaterate(anchorData[1], anchorData[2], anchorData[3]);

                // Send result to connected WebSocket clients
                const payload = JSON.stringify({ position });
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) {
                        client.send(payload);
                    }
                });

                // Reset to wait for next full set of data
                anchorData[1] = null;
                anchorData[2] = null;
                anchorData[3] = null;
            }
        }
    } catch (err) {
        console.error('Failed to parse MQTT message:', err);
    }
});

// Add WebSocket connection logging
wss.on('connection', (ws) => {
    console.log('New WebSocket client connected');
    ws.on('close', () => console.log('WebSocket client disconnected'));
});

app.use(express.static('public'));

server.listen(3000, () => {
    console.log('Server running on http://localhost:3000');
});

// Trilateration logic assuming anchors are at fixed known positions
function trilaterate(d1, d2, d3) {
    // Anchor coordinates in meters (adjust as needed)
    const A = { x: 0, y: 0 };   // Anchor 1
    const B = { x: 6, y: 0 };   // Anchor 2
    const C = { x: 3, y: 4 };   // Anchor 3

    // Formulas from trilateration method
    const A2 = 2 * (B.x - A.x);
    const B2 = 2 * (B.y - A.y);
    const C2 = 2 * (C.x - A.x);
    const D2 = 2 * (C.y - A.y);

    const E2 = d1 ** 2 - d2 ** 2 - A.x ** 2 - A.y ** 2 + B.x ** 2 + B.y ** 2;
    const F2 = d1 ** 2 - d3 ** 2 - A.x ** 2 - A.y ** 2 + C.x ** 2 + C.y ** 2;

    const x = (E2 * D2 - F2 * B2) / (A2 * D2 - C2 * B2);
    const y = (A2 * F2 - C2 * E2) / (A2 * D2 - C2 * B2);

    return {
        x: parseFloat(x.toFixed(2)),
        y: parseFloat(y.toFixed(2))
    };
}

