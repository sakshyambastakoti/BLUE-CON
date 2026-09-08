/**
 * =============================================================================
 * ESP32 SPIDER ROBOT GAMEPAD BRIDGE (BLUEPAD32)
 * =============================================================================
 * Microcontroller: ESP32 Classic / ESP32-WROOM-32 (Bluetooth Classic HID)
 * Gamepad:         DATA FROG S80 / PS4 / PS5 / Xbox One/Series / Switch Pro / 8BitDo
 * Framework:       Arduino IDE (ESP32 Arduino Core)
 * Library:         Bluepad32 by Ricardo Quesada
 * 
 * Communication to Spider Robot (Arduino Nano):
 *  - ESP32 GPIO 17 (TX2) ---> Arduino Nano Pin 0 (RX)
 *  - ESP32 GPIO 16 (RX2) <--- Arduino Nano Pin 1 (TX)
 *  - ESP32 GND           <---> Arduino Nano GND
 *  - Baud Rate: 9600 baud
 * 
 * Features:
 *  - Auto-pairing with Bluetooth Gamepads via Bluepad32
 *  - Radial stick deadzone & priority axis resolution (Forward, Back, Left, Right)
 *  - Edge-triggered button detection (prevents UART command flooding)
 *  - Haptic rumble feedback upon connection, actions, and emergency stop
 *  - Dual output: HardwareSerial2 to Arduino Nano & USB Serial at 115200 for diagnostics
 *  - Optional Web Diagnostic Dashboard (configurable in config.h)
 * =============================================================================
 */

#include <Arduino.h>
#include <Bluepad32.h>
#include <HardwareSerial.h>

#include "config.h"

#if ENABLE_WEB_DASHBOARD
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Update.h>
#include "dashboard_html.h"

const char otaUpdateHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Spider Controller - OTA Firmware Update</title>
  <style>
    body { background: #020611; color: #00f3ff; font-family: monospace; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
    .box { background: rgba(6, 15, 30, 0.85); padding: 35px; border: 1px solid rgba(0, 243, 255, 0.4); border-radius: 10px; box-shadow: 0 0 25px rgba(0, 243, 255, 0.15); text-align: center; max-width: 420px; }
    h2 { margin-top: 0; letter-spacing: 2px; }
    p { color: #5e849c; font-size: 0.9rem; line-height: 1.4; }
    input[type=file] { margin: 25px 0; color: #5e849c; font-size: 0.85rem; }
    input[type=submit] { background: rgba(0, 243, 255, 0.15); border: 1px solid #00f3ff; color: #00f3ff; padding: 12px 24px; cursor: pointer; transition: 0.2s; font-weight: bold; font-family: monospace; border-radius: 4px; }
    input[type=submit]:hover { background: rgba(0, 243, 255, 0.3); box-shadow: 0 0 15px rgba(0, 243, 255, 0.4); }
    a { color: #5e849c; text-decoration: none; margin-top: 25px; display: inline-block; font-size: 0.8rem; }
    a:hover { color: #00f3ff; }
  </style>
</head>
<body>
  <div class="box">
    <h2>[ FIRMWARE UPDATE ]</h2>
    <p>Upload a compiled <code>.bin</code> file to update the ESP32 firmware wirelessly.</p>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update" accept=".bin" required><br>
      <input type="submit" value="UPLOAD & FLASH">
    </form>
    <a href="/">&lt; RETURN TO DASHBOARD</a>
  </div>
</body>
</html>
)rawliteral";
#endif

// =============================================================================
// GLOBAL INSTANCES & STATE
// =============================================================================
HardwareSerial RobotSerial(2); // UART2: TX=17, RX=16

ControllerPtr myController = nullptr;
GamepadState currentState;
GamepadState previousState;

char currentActiveDirection = 'S';
char previousActiveDirection = 'S';
unsigned long lastDirectionSentTime = 0;

#if ENABLE_WEB_DASHBOARD
WebServer server(HTTP_PORT);
WebSocketsServer webSocket = WebSocketsServer(WS_PORT);
unsigned long lastTelemetryBroadcast = 0;
#endif

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
void sendRobotCommand(char cmd, const char* label);
void triggerControllerRumble(uint16_t durationMs, uint8_t weak, uint8_t strong);
void processGamepadMovement();
void processGamepadButtons();
void onConnectedController(ControllerPtr ctl);
void onDisconnectedController(ControllerPtr ctl);

#if ENABLE_WEB_DASHBOARD
void startWiFiNetwork();
void startWebServer();
void startWebSocket();
void broadcastTelemetry();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
#endif

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    // 1. USB Debug Serial
    Serial.begin(DEBUG_BAUD_RATE);
    delay(100);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("   ESP32 SPIDER ROBOT GAMEPAD BRIDGE (BLUEPAD32)  ");
    Serial.println("==================================================");

    // 2. Hardware UART2 to Arduino Nano
    RobotSerial.begin(ROBOT_BAUD, SERIAL_8N1, ROBOT_RX_PIN, ROBOT_TX_PIN);
    Serial.printf("[UART] Robot Serial2 initialized on TX=%d, RX=%d @ %d baud\n",
                  ROBOT_TX_PIN, ROBOT_RX_PIN, ROBOT_BAUD);

    // 3. Clear states
    memset(&currentState, 0, sizeof(GamepadState));
    memset(&previousState, 0, sizeof(GamepadState));
    strncpy(currentState.modelName, "WAITING...", sizeof(currentState.modelName));

    // 4. Initialize Bluepad32
    Serial.println("[BT] Initializing Bluepad32 Bluetooth Gamepad Stack...");
    BP32.setup(&onConnectedController, &onDisconnectedController);

#if ENABLE_WEB_DASHBOARD
    // 5. Initialize Optional Web Diagnostic Server
    Serial.println("[WIFI] Initializing Web Diagnostic Dashboard...");
    startWiFiNetwork();
    startWebServer();
    startWebSocket();
#endif

    Serial.println();
    Serial.println(">>> READY! Put your DATA FROG S80 controller in pairing mode <<<");
    Serial.println(">>> (Press HOME + X or HOME + B to pair) <<<");
    Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    // 1. Process Bluepad32 Bluetooth events (must be called continuously)
    BP32.update();

    // 2. Process Gamepad inputs if connected
    if (myController != nullptr && myController->isConnected() && myController->hasData()) {
        // Ingest raw data
        currentState.connected   = true;
        currentState.axisX       = myController->axisX();
        currentState.axisY       = myController->axisY();
        currentState.axisRX      = myController->axisRX();
        currentState.axisRY      = myController->axisRY();
        currentState.brake       = myController->brake();
        currentState.throttle    = myController->throttle();
        currentState.a           = myController->a();
        currentState.b           = myController->b();
        currentState.x           = myController->x();
        currentState.y           = myController->y();
        currentState.l1          = myController->l1();
        currentState.r1          = myController->r1();
        currentState.l2          = myController->l2() || (currentState.brake > TRIGGER_THRESHOLD);
        currentState.r2          = myController->r2() || (currentState.throttle > TRIGGER_THRESHOLD);
        currentState.thumbL      = myController->thumbL();
        currentState.thumbR      = myController->thumbR();
        currentState.dpad        = myController->dpad();
        currentState.buttons     = myController->buttons();
        currentState.miscButtons = myController->miscButtons();

        // Evaluate motion (sticks & dpad)
        processGamepadMovement();

        // Evaluate action buttons (edge-triggered)
        processGamepadButtons();

        // Save previous state
        previousState = currentState;
    } else {
        currentState.connected = false;
    }

#if ENABLE_WEB_DASHBOARD
    // 3. Handle Web Server & WebSocket Clients
    server.handleClient();
    webSocket.loop();

    unsigned long now = millis();
    if (now - lastTelemetryBroadcast >= TELEMETRY_INTERVAL_MS) {
        lastTelemetryBroadcast = now;
        broadcastTelemetry();
    }
#endif

    delay(2); // Small yield
}

// =============================================================================
// GAMEPAD MOVEMENT PROCESSING (STICKS & D-PAD)
// =============================================================================
void processGamepadMovement() {
    int16_t lx = currentState.axisX;
    int16_t ly = currentState.axisY; // Negative is UP, Positive is DOWN
    uint8_t dpad = currentState.dpad;

    char desiredDirection = 'S';

    // 1. Prioritize D-Pad or Left Stick deflection exceeding deadzone
    bool up    = (dpad & 0x01) || (ly < -STICK_DEADZONE && abs(ly) >= abs(lx));
    bool down  = (dpad & 0x02) || (ly >  STICK_DEADZONE && abs(ly) >= abs(lx));
    bool left  = (dpad & 0x08) || (lx < -STICK_DEADZONE && abs(lx) >  abs(ly));
    bool right = (dpad & 0x04) || (lx >  STICK_DEADZONE && abs(lx) >  abs(ly));

    if (up) {
        desiredDirection = 'F'; // Walk Forward
    } else if (down) {
        desiredDirection = 'B'; // Walk Backward
    } else if (left) {
        desiredDirection = 'L'; // Turn Left
    } else if (right) {
        desiredDirection = 'R'; // Turn Right
    } else {
        desiredDirection = 'S'; // Stop / Neutral
    }

    // 2. State Transition Logic
    if (desiredDirection != currentActiveDirection) {
        // When changing between two non-stop directions, send 'S' to cleanly reset stride
        if (currentActiveDirection != 'S' && desiredDirection != 'S') {
            sendRobotCommand('S', "TRANSITION_STOP");
            delay(15);
        }

        const char* label = "STOP";
        if (desiredDirection == 'F') label = "FORWARD";
        else if (desiredDirection == 'B') label = "BACKWARD";
        else if (desiredDirection == 'L') label = "TURN_LEFT";
        else if (desiredDirection == 'R') label = "TURN_RIGHT";

        sendRobotCommand(desiredDirection, label);
        currentActiveDirection = desiredDirection;
        lastDirectionSentTime = millis();
    } 
    // 3. Periodic Keep-Alive Heartbeat if user continues to hold stick
    else if (currentActiveDirection != 'S' && (millis() - lastDirectionSentTime >= HEARTBEAT_INTERVAL_MS)) {
        sendRobotCommand(currentActiveDirection, "HEARTBEAT");
        lastDirectionSentTime = millis();
    }
}

// =============================================================================
// GAMEPAD ACTION BUTTON PROCESSING (EDGE-TRIGGERED)
// =============================================================================
void processGamepadButtons() {
    // Button A -> Hand Shake ('U')
    if (currentState.a && !previousState.a) {
        sendRobotCommand('U', "HAND_SHAKE");
        triggerControllerRumble(150, 100, 100);
    }

    // Button B -> Hand Wave ('W')
    if (currentState.b && !previousState.b) {
        sendRobotCommand('W', "HAND_WAVE");
        triggerControllerRumble(150, 100, 100);
    }

    // Button X -> Basic Standing Stance ('P')
    if (currentState.x && !previousState.x) {
        sendRobotCommand('P', "BASIC_STANCE");
        triggerControllerRumble(100, 80, 80);
    }

    // Button Y -> Spider Combat Stance ('Q')
    if (currentState.y && !previousState.y) {
        sendRobotCommand('Q', "SPIDER_STANCE");
        triggerControllerRumble(100, 80, 80);
    }

    // Shoulder L1 -> Body Dance ('V')
    if (currentState.l1 && !previousState.l1) {
        sendRobotCommand('V', "BODY_DANCE");
        triggerControllerRumble(300, 180, 180);
    }

    // Shoulder R1 -> Eye LED Blink ('K')
    if (currentState.r1 && !previousState.r1) {
        sendRobotCommand('K', "LED_BLINK");
        triggerControllerRumble(100, 100, 0);
    }

    // Trigger L2 (ZL) -> Eye LED Off ('X')
    if (currentState.l2 && !previousState.l2) {
        sendRobotCommand('X', "LED_OFF");
    }

    // Trigger R2 (ZR) -> Eye LED On ('O')
    if (currentState.r2 && !previousState.r2) {
        sendRobotCommand('O', "LED_ON");
    }

    // Right Stick Click (R3) -> Emergency Stop ('S')
    if (currentState.thumbR && !previousState.thumbR) {
        currentActiveDirection = 'S';
        sendRobotCommand('S', "EMERGENCY_STOP_R3");
        triggerControllerRumble(400, 255, 255);
    }

    // Left Stick Click (L3) -> Stance Toggle ('Q' or 'P')
    if (currentState.thumbL && !previousState.thumbL) {
        sendRobotCommand('Q', "STANCE_TOGGLE");
        triggerControllerRumble(100, 80, 80);
    }

    // Misc Buttons (START / SELECT / HOME)
    if (currentState.miscButtons != previousState.miscButtons) {
        // Start / Plus button
        if (currentState.miscButtons & 0x0004) {
            sendRobotCommand('V', "START_DANCE");
            triggerControllerRumble(250, 150, 150);
        }
        // Select / Minus button
        if (currentState.miscButtons & 0x0002) {
            sendRobotCommand('P', "RESET_STANCE");
            triggerControllerRumble(150, 100, 100);
        }
        // System / Home button -> Emergency Stop
        if (currentState.miscButtons & 0x0001) {
            currentActiveDirection = 'S';
            sendRobotCommand('S', "EMERGENCY_STOP_HOME");
            triggerControllerRumble(400, 255, 255);
        }
    }
}

// =============================================================================
// SERIAL UART COMMAND DISPATCH
// =============================================================================
void sendRobotCommand(char cmd, const char* label) {
    // Send 1 byte command over HardwareSerial2 to Arduino Nano
    RobotSerial.write(cmd);

    // Print to USB serial for monitoring
    Serial.printf("[CMD -> NANO] '%c'  [%s]\n", cmd, label);
}

// =============================================================================
// HAPTIC RUMBLE FEEDBACK
// =============================================================================
void triggerControllerRumble(uint16_t durationMs, uint8_t weak, uint8_t strong) {
    if (myController != nullptr && myController->isConnected()) {
        myController->playDualRumble(0, durationMs, weak, strong);
    }
}

// =============================================================================
// BLUEPAD32 CONTROLLER CALLBACKS
// =============================================================================
void onConnectedController(ControllerPtr ctl) {
    myController = ctl;
    currentState.connected = true;
    currentState.index = ctl->index();
    strncpy(currentState.modelName, ctl->getModelName().c_str(), sizeof(currentState.modelName) - 1);

    Serial.println();
    Serial.println("==================================================");
    Serial.printf("[GAMEPAD CONNECTED] Model: %s (Index: %d)\n", 
                  ctl->getModelName().c_str(), ctl->index());
    Serial.println("==================================================");

    // Set Neon Cyan Player LED
    ctl->setColorLED(0, 255, 255);

    // Double vibration pulse to greet the pilot
    triggerControllerRumble(300, 200, 200);

    // Ensure robot is in initial safe stop stance
    sendRobotCommand('S', "INITIAL_STOP");
}

void onDisconnectedController(ControllerPtr ctl) {
    if (myController == ctl) {
        myController = nullptr;
        currentState.connected = false;
        strncpy(currentState.modelName, "DISCONNECTED", sizeof(currentState.modelName));

        Serial.println();
        Serial.println("==================================================");
        Serial.println("[GAMEPAD DISCONNECTED] Safe stop sent to robot!");
        Serial.println("==================================================");

        // Safety: Halt robot immediately
        currentActiveDirection = 'S';
        sendRobotCommand('S', "FAILSAFE_DISCONNECT_STOP");
    }
}

#if ENABLE_WEB_DASHBOARD
// =============================================================================
// OPTIONAL WI-FI & WEB DASHBOARD INTEGRATION
// =============================================================================
void startWiFiNetwork() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < 6000)) {
        delay(400);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected to '%s'! IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("[WIFI] Using SoftAP: SSID '%s' (PW: %s) @ http://%s\n", 
                      AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    }
}

void startWebServer() {
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", DASHBOARD_HTML);
    });
    server.on("/cmd", HTTP_GET, []() {
        if (server.hasArg("go") && server.arg("go").length() > 0) {
            char c = server.arg("go")[0];
            sendRobotCommand(c, "WEB_OVERRIDE");
            server.send(200, "text/plain", String("OK:") + c);
        } else {
            server.send(400, "text/plain", "Missing cmd");
        }
    });
    server.on("/update", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send_P(200, "text/html", otaUpdateHtml);
    });
    server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "UPDATE FAILED!" : "UPDATE SUCCESS! Rebooting...");
        delay(1000);
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("[OTA] Flashing firmware: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.printf("[OTA] Update Success: %u bytes! Rebooting...\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        }
    });

    server.onNotFound([]() {
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "Redirecting...");
    });
    server.begin();
    Serial.printf("[HTTP] Web Server started on port %d\n", HTTP_PORT);
}

void startWebSocket() {
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.printf("[WS] WebSocket Server started on port %d\n", WS_PORT);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) {
        broadcastTelemetry();
    }
}

void broadcastTelemetry() {
    if (webSocket.connectedClients() == 0) return;

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
#endif
