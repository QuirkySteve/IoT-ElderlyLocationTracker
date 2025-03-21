// Install dependencies: npm install express ws mqtt
const express = require('express');
const WebSocket = require('ws');
const mqtt = require('mqtt');

const app = express();
const server = require('http').createServer(app);
const wss = new WebSocket.Server({ server });

const MQTT_BROKER = 'mqtt://192.168.183.19'; // Note the double slashes
const MQTT_TOPIC = 'ble/anchors'; // Unified topic from all anchors

const mqttClient = mqtt.connect(MQTT_BROKER);

// Store latest distances from anchors
const anchorData = {};

mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
    mqttClient.subscribe(MQTT_TOPIC);
});

mqttClient.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
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
    } catch (err) {
        console.error('Failed to parse MQTT message:', err);
    }
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
