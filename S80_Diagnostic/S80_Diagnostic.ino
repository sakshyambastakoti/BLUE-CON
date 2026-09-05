/**
 * =============================================================================
 * DATA FROG S80 → ESP32 → Real-Time Web Diagnostic Dashboard
 * =============================================================================
 * Microcontroller: ESP32 Classic / ESP32-WROOM-32 (Bluetooth Classic enabled)
 * Gamepad:         DATA FROG S80 Wireless Bluetooth Game Controller
 * Framework:       Arduino IDE (ESP32 Arduino Core)
 * Libraries:       Bluepad32 (Ricardo Quesada)
 *                  WebSockets by Markus Sattler (Links2004)
 *                  WiFi & WebServer (ESP32 Built-in)
 * =============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Bluepad32.h>

#include "config.h"
#include "dashboard_html.h"

// =============================================================================
// GLOBAL INSTANCES & STATE
// =============================================================================
WebServer server(HTTP_PORT);
WebSocketsServer webSocket = WebSocketsServer(WS_PORT);

ControllerPtr myControllers[BP32_MAX_GAMEPADS];
GamepadState currentState;
GamepadState lastLoggedState;

unsigned long lastTelemetryTime = 0;
unsigned long lastSerialLogTime = 0;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
void startWiFi();
void startWebServer();
void startWebSocket();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void onConnectedController(ControllerPtr ctl);
void onDisconnectedController(ControllerPtr ctl);
void readControllerData();
void sendControllerData();
void printPeriodicSerialDiagnostic();

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(100);

    Serial.println();
    Serial.println("========================================");
    Serial.println("DATA FROG S80 DIAGNOSTIC SYSTEM");
    Serial.println("========================================");

    // 1. Initialize Bluepad32 Bluetooth Stack
    Serial.println("Initializing Bluepad32...");
    BP32.setup(&onConnectedController, &onDisconnectedController);
    // BP32.forgetBluetoothKeys(); // Uncomment if you need to wipe existing Bluetooth pairings

    // 2. Initialize Wi-Fi Soft Access Point
    Serial.println("Initializing WiFi AP...");
    startWiFi();

    // 3. Initialize Web Server & WebSockets
    Serial.println("Initializing Web Server & WebSockets...");
    startWebServer();
    startWebSocket();

    // Reset state
    memset(&currentState, 0, sizeof(GamepadState));
    memset(&lastLoggedState, 0, sizeof(GamepadState));
    strncpy(currentState.modelName, "NONE", sizeof(currentState.modelName));

    Serial.println();
    Serial.println("========================================");
    Serial.println("S80 WEB DIAGNOSTIC READY");
    Serial.println("========================================");
    Serial.printf("SSID:     %s\n", AP_SSID);
    Serial.printf("Password: %s\n", AP_PASSWORD);
    Serial.print ("IP:       "); Serial.println(WiFi.softAPIP());
    Serial.printf("Dashboard URL: http://%s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("========================================");
    Serial.println("Waiting for controller...");
    Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    // 1. Process Bluepad32 controller events (must be called every loop)
    BP32.update();

    // 2. Handle HTTP client requests
    server.handleClient();

    // 3. Handle WebSocket traffic
    webSocket.loop();

    // 4. Ingest latest inputs from active controller
    readControllerData();

    // 5. Broadcast real-time telemetry (non-blocking 30-40 Hz timer)
    unsigned long now = millis();
    if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryTime = now;
        sendControllerData();
    }

    // 6. Compact periodic Serial diagnostic (every 3 seconds when connected)
    if (now - lastSerialLogTime >= 3000) {
        lastSerialLogTime = now;
        printPeriodicSerialDiagnostic();
    }
}

// =============================================================================
// WI-FI ACCESS POINT INITIALIZATION
// =============================================================================
void startWiFi() {
    WiFi.mode(WIFI_AP);
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS);

    if (apStarted) {
        IPAddress apIP = WiFi.softAPIP();
        Serial.print("WiFi AP Started. Local IP: ");
        Serial.println(apIP);
    } else {
        Serial.println("ERROR: Failed to initialize WiFi AP!");
    }
}

// =============================================================================
// WEB SERVER INITIALIZATION
// =============================================================================
void startWebServer() {
    // Serve embedded HTML5 diagnostic dashboard from Flash PROGMEM
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", DASHBOARD_HTML);
    });

    // Captive portal fallback
    server.onNotFound([]() {
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "Redirecting to S80 Dashboard...");
    });

    server.begin();
    Serial.printf("HTTP Web Server started on port %d\n", HTTP_PORT);
}

// =============================================================================
// WEBSOCKET INITIALIZATION & EVENT HANDLER
// =============================================================================
void startWebSocket() {
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("WebSocket Server started on port %d\n", WS_PORT);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            // Client browser disconnected
            break;

        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[WS] Client #%u connected from %s\n", num, ip.toString().c_str());
            // Send current state to newly connected client immediately
            sendControllerData();
            break;
        }

        case WStype_TEXT:
            // Optional: Handle incoming messages from browser dashboard
            break;

        default:
            break;
    }
}

// =============================================================================
// BLUEPAD32 CONTROLLER CALLBACKS
// =============================================================================
void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            myControllers[i] = ctl;
            foundEmptySlot = true;

            Serial.println();
            Serial.println("----------------------------------------");
            Serial.println("S80 Connected");
            Serial.printf("Controller Index: %d\n", ctl->index());
            Serial.printf("Model:            %s\n", ctl->getModelName().c_str());
            Serial.println("----------------------------------------");
            Serial.println();
            break;
        }
    }

    if (!foundEmptySlot) {
        Serial.println("WARNING: Controller connected, but max gamepads limit reached!");
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            myControllers[i] = nullptr;

            Serial.println();
            Serial.println("----------------------------------------");
            Serial.println("S80 Disconnected");
            Serial.println("----------------------------------------");
            Serial.println();

            // Clear state
            currentState.connected = false;
            strncpy(currentState.modelName, "NONE", sizeof(currentState.modelName));
            sendControllerData();
            break;
        }
    }
}

// =============================================================================
// CONTROLLER DATA PROCESSING
// =============================================================================
void readControllerData() {
    ControllerPtr ctl = nullptr;

    // Retrieve active connected controller
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] != nullptr && myControllers[i]->isConnected()) {
            ctl = myControllers[i];
            break;
        }
    }

    if (ctl != nullptr && ctl->hasData()) {
        currentState.connected = true;
        currentState.index = ctl->index();
        strncpy(currentState.modelName, ctl->getModelName().c_str(), sizeof(currentState.modelName) - 1);

        // Analog sticks (Approx. -512 to +512)
        currentState.axisX  = ctl->axisX();
        currentState.axisY  = ctl->axisY();
        currentState.axisRX = ctl->axisRX();
        currentState.axisRY = ctl->axisRY();

        // Analog triggers (Approx. 0 to 1020)
        currentState.brake    = ctl->brake();
        currentState.throttle = ctl->throttle();

        // Main action buttons
        currentState.a  = ctl->a();
        currentState.b  = ctl->b();
        currentState.x  = ctl->x();
        currentState.y  = ctl->y();

        // Bumpers and triggers
        currentState.l1 = ctl->l1();
        currentState.r1 = ctl->r1();
        currentState.l2 = ctl->l2();
        currentState.r2 = ctl->r2();

        // Stick Click Buttons (L3 / R3)
        // Bluepad32 defines thumbL() and thumbR(), or bitwise check on buttons()
        currentState.thumbL = ctl->thumbL();
        currentState.thumbR = ctl->thumbR();

        // Directional Pad (UP=0x01, DOWN=0x02, RIGHT=0x04, LEFT=0x08)
        currentState.dpad = ctl->dpad();

        // Raw bitmasks
        currentState.buttons     = ctl->buttons();
        currentState.miscButtons = ctl->miscButtons();
    } else {
        currentState.connected = false;
    }
}

// =============================================================================
// TELEMETRY TRANSMISSION (WEBSOCKET JSON BROADCAST)
// =============================================================================
void sendControllerData() {
    // Only broadcast if there are connected WebSocket clients
    if (webSocket.connectedClients() == 0) return;

    // Format fast, compact JSON in fixed static buffer (no heap fragmentation)
    char jsonBuffer[384];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"c\":%d,\"idx\":%d,\"name\":\"%s\","
        "\"lx\":%d,\"ly\":%d,\"rx\":%d,\"ry\":%d,"
        "\"zl\":%d,\"zr\":%d,\"btn\":%u,\"misc\":%u,\"dpad\":%u,"
        "\"a\":%d,\"b\":%d,\"x\":%d,\"y\":%d,"
        "\"l1\":%d,\"r1\":%d,\"l2\":%d,\"r2\":%d,"
        "\"l3\":%d,\"r3\":%d}",
        currentState.connected ? 1 : 0,
        currentState.index,
        currentState.modelName,
        currentState.axisX,
        currentState.axisY,
        currentState.axisRX,
        currentState.axisRY,
        currentState.brake,
        currentState.throttle,
        currentState.buttons,
        currentState.miscButtons,
        currentState.dpad,
        currentState.a ? 1 : 0,
        currentState.b ? 1 : 0,
        currentState.x ? 1 : 0,
        currentState.y ? 1 : 0,
        currentState.l1 ? 1 : 0,
        currentState.r1 ? 1 : 0,
        currentState.l2 ? 1 : 0,
        currentState.r2 ? 1 : 0,
        currentState.thumbL ? 1 : 0,
        currentState.thumbR ? 1 : 0
    );

    webSocket.broadcastTXT(jsonBuffer);
}

// =============================================================================
// PERIODIC SERIAL DIAGNOSTIC MONITOR
// =============================================================================
void printPeriodicSerialDiagnostic() {
    if (currentState.connected) {
        Serial.printf("[S80 STATUS] LX:%4d LY:%4d | RX:%4d RY:%4d | ZL:%4d ZR:%4d | BTN:0x%04X MISC:0x%04X DPAD:0x%02X | Clients:%u\n",
            currentState.axisX, currentState.axisY,
            currentState.axisRX, currentState.axisRY,
            currentState.brake, currentState.throttle,
            currentState.buttons, currentState.miscButtons, currentState.dpad,
            webSocket.connectedClients()
        );
    }
}
