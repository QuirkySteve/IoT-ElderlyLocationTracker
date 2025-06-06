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

const MQTT_BROKER = 'mqtt://192.168.139.19'; // Note the double slashes
const BLE_TOPIC = 'ble/anchors'; // Topic for BLE data

const mqttClient = mqtt.connect(MQTT_BROKER);

// Store latest distances and timestamps from anchors
const anchorData = {
    1: { distance: null, timestamp: 0 },
    2: { distance: null, timestamp: 0 },
    3: { distance: null, timestamp: 0 }
};
// Store latest RSSI and timestamps from LoRa rooms
const loraRoomData = {
    'D': { rssi: null, timestamp: 0 }, // Room D
    'E': { rssi: null, timestamp: 0 }  // Room E
};
const LORA_DATA_TIMEOUT_MS = 15000; // Consider LoRa data older than 15s stale
const DATA_TIMEOUT_MS = 10000; // Optional: Consider data older than 10s stale

mqttClient.on('connect', () => {
    console.log('Connected to MQTT broker');
    mqttClient.subscribe(BLE_TOPIC);
});

// Handle LoRa messages - New logic based on combined room state
loraHandler.on('lora-data', (data) => {
    console.log('Received LoRa data:', data);
    const now = Date.now();
    const sender = data.sender; // 'D' or 'E'
    const rssi = data.rssi;

    // Update the specific room's data
    if (loraRoomData.hasOwnProperty(sender)) {
        loraRoomData[sender] = { rssi: rssi, timestamp: now };
    } else {
        console.warn(`Received LoRa data from unknown sender: ${sender}`);
        return;
    }

    // Get latest data for both rooms, checking for staleness
    const roomD = loraRoomData['D'];
    const roomE = loraRoomData['E'];

    const isDValid = roomD.rssi !== null && (now - roomD.timestamp < LORA_DATA_TIMEOUT_MS);
    const isEValid = roomE.rssi !== null && (now - roomE.timestamp < LORA_DATA_TIMEOUT_MS);

    const rssiD = isDValid ? roomD.rssi : null;
    const rssiE = isEValid ? roomE.rssi : null;

    let calculatedStatus = 'Unknown';
    const TRANSIT_LOW_THRESHOLD = -90;
    const TRANSIT_HIGH_THRESHOLD = -65; // New threshold from user
    const OUT_THRESHOLD = -90;

    // Determine status based on combined valid data with REVISED priority
    // Priority 1: In Room (At least one signal is strong)
    if ((isDValid && rssiD > TRANSIT_HIGH_THRESHOLD) || (isEValid && rssiE > TRANSIT_HIGH_THRESHOLD)) {
        if (isDValid && isEValid) {
            // Both valid and at least one is strong, compare them
            calculatedStatus = (rssiD >= rssiE) ? 'In Room D' : 'In Room E'; // Higher RSSI wins
        } else if (isDValid) {
            // Only D is valid and it's strong
            calculatedStatus = 'In Room D';
        } else {
            // Only E is valid and it's strong
            calculatedStatus = 'In Room E';
        }
    }
    // Priority 2: Transit (Not In Room, but at least one signal is in transit range)
    else if ((isDValid && rssiD > TRANSIT_LOW_THRESHOLD) || (isEValid && rssiE > TRANSIT_LOW_THRESHOLD)) {
         // Check if the *other* signal makes it Out of Facility (overrides Transit)
         // Note: We already know neither signal is > TRANSIT_HIGH_THRESHOLD from the check above
         if (isDValid && rssiD <= OUT_THRESHOLD) {
             calculatedStatus = 'Out of Facility'; // If D is OOF, overall is OOF
         } else if (isEValid && rssiE <= OUT_THRESHOLD) {
             calculatedStatus = 'Out of Facility'; // If E is OOF, overall is OOF
         } else {
            calculatedStatus = 'Transit'; // Otherwise, it's Transit
         }
    }
    // Priority 3: Out of Facility (Not In Room or Transit, and both signals are weak)
    else if (isDValid && isEValid && rssiD <= OUT_THRESHOLD && rssiE <= OUT_THRESHOLD) {
        calculatedStatus = 'Out of Facility';
    }
    // Handle cases where only one signal is valid and it's weak (OOF)
    else if (isDValid && rssiD <= OUT_THRESHOLD) {
        calculatedStatus = 'Out of Facility';
    }
    else if (isEValid && rssiE <= OUT_THRESHOLD) {
        calculatedStatus = 'Out of Facility';
    }
    // If none of the above, status remains 'Unknown'

    // Prepare payload with combined status and individual room data
    const payload = {
        type: 'lora_status', // New type
        status: calculatedStatus,
        roomD: { rssi: rssiD, timestamp: isDValid ? new Date(roomD.timestamp).toLocaleTimeString() : 'Never' }, // Send formatted time
        roomE: { rssi: rssiE, timestamp: isEValid ? new Date(roomE.timestamp).toLocaleTimeString() : 'Never' }  // Send formatted time
    };

    console.log('Determined LoRa Status:', calculatedStatus, 'Payload:', payload); // Log determined status

    // Send to all connected WebSocket clients
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify(payload));
        }
    });
});

mqttClient.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        
        // Get receive timestamp
        const receivedAt = new Date().toISOString(); // Broker receive time

        console.log(`📦 MQTT Payload from Anchor ${data.anchor}`);
        console.log(`🔹 Sent at    : ${data.timestamp}`);
        console.log(`🔹 Received at: ${receivedAt}`);

        // Now you can also store or compare these if needed
        // Example: calculate latency in ms if both are ISO strings
        const sentTime = new Date(data.timestamp);
        const recvTime = new Date(receivedAt);
        const latencyMs = recvTime - sentTime;
        console.log(`⏱️  Approx. Latency: ${latencyMs} ms`);

        if (topic === BLE_TOPIC) {
            const anchorId = parseInt(data.anchor);
            const distance = parseFloat(data.distance);
            const now = Date.now();

            // Update the specific anchor's data
            if (anchorData.hasOwnProperty(anchorId)) {
                anchorData[anchorId].distance = distance;
                anchorData[anchorId].timestamp = now;
                console.log(`Received from Anchor ${anchorId}: ${distance.toFixed(2)}m`); // Log received data
            } else {
                console.warn(`Received data for unknown anchor ID: ${anchorId}`);
                return; // Skip if anchor ID is invalid
            }

            // Check if we have at least one reading from all anchors
            // Optional: Add timeout check: && (now - anchorData[id].timestamp < DATA_TIMEOUT_MS)
            const canCalculate = Object.values(anchorData).every(anchor => anchor.distance !== null);

            if (canCalculate) {
                const d1 = anchorData[1].distance;
                const d2 = anchorData[2].distance;
                const dist1 = anchorData[1].distance; // Distance from Anchor 1
                const dist2 = anchorData[2].distance; // Distance from Anchor 2
                const dist3 = anchorData[3].distance; // Distance from Anchor 3 (Origin for calculation)

                console.log(`Attempting trilateration with distances: Anchor1=${dist1?.toFixed(2)}, Anchor2=${dist2?.toFixed(2)}, Anchor3=${dist3?.toFixed(2)}`); // Log distances used

                // Pass distances in the order: d_Origin(A=3), d_B(B=1), d_C(C=2)
                const position = trilaterate(dist3, dist1, dist2);

                // The calculated position is relative to Anchor 3's location (0.425, 0)
                // Adjust to be relative to the bottom-left (0,0) origin of the paper
                const adjustedPosition = {
                    x: position.x + 2.5, // 2.5m offset in X from Anchor 3
                    y: position.y        // Y stays the same
                  };                  

                // Clamp adjusted position to 5m x 5m grid
                adjustedPosition.x = Math.min(Math.max(0, adjustedPosition.x), 5);
                adjustedPosition.y = Math.min(Math.max(0, adjustedPosition.y), 5);

                console.log(`Calculated position (relative to Anchor 3): x=${position.x.toFixed(2)}, y=${position.y.toFixed(2)}`); // Log raw calculated position
                console.log(`Adjusted position (relative to paper origin 0,0): x=${adjustedPosition.x.toFixed(2)}, y=${adjustedPosition.y.toFixed(2)}`); // Log final position

                // Send adjusted result to connected WebSocket clients
                const payload = JSON.stringify({
                    position: adjustedPosition,
                    sentAt: data.timestamp, // from anchor payload
                    receivedAt: receivedAt,
                    latency: latencyMs
                  });
                  
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) {
                        client.send(payload); // Send the adjusted position
                    }
                });

                // No longer reset data - keep last known values
                // anchorData[1] = null; // Removed
                // anchorData[2] = null; // Removed
                // anchorData[3] = null; // Removed
            } else {
                 console.log("Waiting for data from all anchors...");
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

// Trilateration logic using Anchor 3 as the origin (A) for calculation
// Input distances d1, d2, d3 correspond to distances from A(3), B(1), C(2) respectively
function trilaterate(d1, d2, d3) {
    // Anchor coordinates relative to Anchor 3 (A) in meters
    const A = { x: 0,     y: 0 };     // Anchor 3 at (2.5, 0)
    const B = { x: -2.5,  y: 5.0 };   // Anchor 1 at (0, 5)
    const C = { x: 2.5,   y: 5.0 };   // Anchor 2 at (5, 5)


    // Check for invalid distances (e.g., null, undefined, negative)
    if (d1 == null || d2 == null || d3 == null || d1 < 0 || d2 < 0 || d3 < 0) {
        console.error("Trilateration error: Invalid distance input.", {d1, d2, d3});
        return { x: 0, y: 0 }; // Return default or last known position
    }

    // Formulas from trilateration method (using A as origin)
    const A2 = 2 * B.x; // 2 * (B.x - A.x) = 2 * (B.x - 0)
    const B2 = 2 * B.y; // 2 * (B.y - A.y) = 2 * (B.y - 0)
    const C2 = 2 * C.x; // 2 * (C.x - A.x) = 2 * (C.x - 0)
    const D2 = 2 * C.y; // 2 * (C.y - A.y) = 2 * (C.y - 0)

    const E2 = d1 ** 2 - d2 ** 2 + B.x ** 2 + B.y ** 2; // d1^2 - d2^2 - A.x^2 - A.y^2 + B.x^2 + B.y^2 = d1^2 - d2^2 + B.x^2 + B.y^2
    const F2 = d1 ** 2 - d3 ** 2 + C.x ** 2 + C.y ** 2; // d1^2 - d3^2 - A.x^2 - A.y^2 + C.x^2 + C.y^2 = d1^2 - d3^2 + C.x^2 + C.y^2

    // Calculate denominator
    const denominator = (A2 * D2 - C2 * B2);
    if (Math.abs(denominator) < 1e-6) { // Avoid division by zero or near-zero
         console.error("Trilateration error: Denominator too small (anchors might be collinear or distances inconsistent).", {A2, B2, C2, D2});
         return { x: 0, y: 0 }; // Or return last known good position
    }

    const x = (E2 * D2 - F2 * B2) / denominator;
    const y = (A2 * F2 - C2 * E2) / denominator;

    return {
        x: parseFloat(x.toFixed(2)), // Return value relative to Anchor 3
        y: parseFloat(y.toFixed(2))
    };
}
