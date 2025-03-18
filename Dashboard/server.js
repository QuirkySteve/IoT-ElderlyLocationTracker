// Install dependencies: npm install express ws mqtt
const express = require('express');
const WebSocket = require('ws');
const mqtt = require('mqtt');

const app = express();
const server = require('http').createServer(app);
const wss = new WebSocket.Server({ server });

const MQTT_BROKER = 'mqtt:172.20.10.2'; // Change as needed
const MQTT_TOPICS = ['ble/anchor/+'];

const mqttClient = mqtt.connect(MQTT_BROKER);

mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
    mqttClient.subscribe(MQTT_TOPICS);
});

mqttClient.on('message', (topic, message) => {
    console.log(`MQTT Message from ${topic}: ${message.toString()}`);
    
    const match = topic.match(/ble\/anchor\/(\d+)/); // Extract anchor ID
    if (match) {
        const anchorId = match[1]; // Get the anchor number
        wss.clients.forEach(client => {
            if (client.readyState === WebSocket.OPEN) {
                client.send(JSON.stringify({ anchorId, message: message.toString() }));
            }
        });
    }
});


app.use(express.static('public'));

server.listen(3000, () => console.log('Server running on http://localhost:3000'));
