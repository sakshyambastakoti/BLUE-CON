# DATA FROG S80 → ESP32 Real-Time Diagnostic Dashboard

A professional, real-time input diagnostic system and web dashboard for the **DATA FROG S80 Wireless Bluetooth Game Controller** connected directly to an **ESP32 Classic (ESP32-WROOM-32)** running **Bluepad32**.

---

## 1. System Architecture

```
 ┌───────────────────────────┐
 │   DATA FROG S80 Gamepad   │
 └─────────────┬─────────────┘
               │ Bluetooth Classic (HID)
               ▼
 ┌───────────────────────────┐
 │       ESP32 WROOM-32      │
 │  - Bluepad32 Stack        │
 │  - Wi-Fi AP (192.168.4.1) │
 │  - HTTP Server (Port 80)  │
 │  - WebSocket (Port 81)    │
 └─────────────┬─────────────┘
               │ WebSocket Broadcast (33 Hz / 30ms)
               ▼
 ┌───────────────────────────┐
 │   Client Web Browser      │
 │  - Interactive Joysticks  │
 │  - Trigger Gauges         │
 │  - Tactile Button Matrix  │
 │  - D-Pad Hat Switch       │
 │  - Raw Hex Monitor        │
 │  - Input Activity Log     │
 └───────────────────────────┘
```

---

## 2. Hardware Requirements

* **Microcontroller**: ESP32 Classic / ESP32-WROOM-32 (NodeMCU-32S, ESP32 DevKit v1, etc.).
  * *Note: ESP32-S3 and ESP32-C3 are NOT supported because Bluetooth Classic HID is required for this controller.*
* **Gamepad**: DATA FROG S80 Wireless Bluetooth Controller.
* **Client Device**: Any Smartphone, Tablet, or PC with Wi-Fi and a modern web browser.

---

## 3. Software & Library Requirements

In **Arduino IDE**:

1. **ESP32 Board Package**:
   * Tools → Board → Boards Manager → search `esp32` by Espressif Systems (v2.0.x or v3.0.x).
2. **Bluepad32**:
   * Tools → Manage Libraries → search `Bluepad32` by Ricardo Quesada → Install latest.
3. **WebSockets**:
   * Tools → Manage Libraries → search `WebSockets` by Markus Sattler (Links2004) → Install latest.
4. **Built-in Libraries** (included with ESP32 core):
   * `<WiFi.h>`
   * `<WebServer.h>`

---

## 4. File Structure

```
d:/BLUE-CON/S80_Diagnostic/
├── S80_Diagnostic.ino    # Main sketch: Bluepad32 callbacks, Wi-Fi AP, WebServer, WebSockets
├── config.h            # AP credentials, port assignments, GamepadState data model
├── dashboard_html.h     # Complete HTML5/CSS3/JS Cyberpunk telemetry dashboard (in PROGMEM)
└── README.md           # Setup and operation guide
```

---

## 5. Quick Start Guide

### Step 1: Configure & Upload
1. Open `S80_Diagnostic.ino` in Arduino IDE.
2. Select your board: **Tools → Board → esp32 → ESP32 Dev Module**.
3. Select the correct COM port: **Tools → Port**.
4. Set Upload Speed to `921600` (or `115200` if upload fails).
5. Click **Upload**.

### Step 2: Open Serial Monitor
1. Set baud rate to **115200 baud**.
2. Press the `EN` / `RST` button on the ESP32.
3. You will see:
```text
========================================
DATA FROG S80 DIAGNOSTIC SYSTEM
========================================
Initializing Bluepad32...
Initializing WiFi AP...
Connecting to Wi-Fi network 'sakshyam' .....
Connected to 'sakshyam'! Local IP: 192.168.1.50
Initializing Web Server & WebSockets...
HTTP Web Server started on port 80
WebSocket Server started on port 81

========================================
S80 WEB DIAGNOSTIC READY
========================================
Wi-Fi Network: Connected to 'sakshyam'
Local IP:      192.168.1.50
Dashboard URL: http://192.168.1.50
----------------------------------------
Fallback Direct AP also available:
SSID:          S80-DIAGNOSTIC (PW: 12345678)
AP URL:        http://192.168.4.1
========================================
Waiting for controller...
```

### Step 3: Pair the DATA FROG S80 Controller
* **Nintendo Switch Pairing Mode**:
  * Press and hold the **SYNC** button on the back of the controller for ~2 seconds until the LED indicators cycle rapidly.
* **Android HID Pairing Mode**:
  * Hold **A + HOME** until LED1 and LED2 flash.
* Once paired, the ESP32 Serial Monitor will log:
```text
----------------------------------------
S80 Connected
Controller Index: 0
Model:            DATA FROG S80
----------------------------------------
```

### Step 4: Open the Dashboard
You have **two ways** to access the live dashboard:

* **Option A (Via Your Wi-Fi Network)**:
  1. Make sure your phone or laptop is connected to your regular Wi-Fi network (`sakshyam`).
  2. Open your browser and go to the ESP32 Local IP address printed in the Serial Monitor:
     * `http://<ESP32_Local_IP>` (e.g., `http://192.168.1.50`)

* **Option B (Direct Access Point)**:
  1. Connect your phone or laptop directly to the ESP32 AP:
     * **SSID**: `S80-DIAGNOSTIC`
     * **Password**: `12345678`
  2. Open your browser and navigate to:
     * `http://192.168.4.1`

---

## 6. Dashboard Features & Sections

### A. Connection Status Bar
* **Pulsing Indicator**: Green glowing dot for `CONNECTED`, red for `DISCONNECTED`.
* **Telemetry Hz**: Live rate counter showing real-time updates per second (target 30–35 Hz).
* **Controller Identity**: Model name and gamepad index.

### B. Dual 2D Analog Joysticks
* **Left Stick (`axisX`, `axisY`)** & **Right Stick (`axisRX`, `axisRY`)**:
  * Full 2D coordinate plane with deadzone indicator ring and coordinate crosshair.
  * Real-time moving thumbstick indicator.
  * Directional tags: `UP`, `DOWN`, `LEFT`, `RIGHT`, `CENTER` highlighted based on deadzone.
  * Live numerical readout (`-512` to `+512`).

### C. Analog Triggers
* **ZL / Brake** and **ZR / Throttle**:
  * Glowing dynamic level gauges with scale markers (`0`, `255`, `512`, `768`, `1020`).
  * Live raw numerical values (`0` to `~1020`).

### D. Main Button Matrix
* **Buttons**: `A`, `B`, `X`, `Y`, `L`, `R`, `ZL`, `ZR`, `L3` (Left Thumb Click), `R3` (Right Thumb Click).
* Tactile visual depression and neon color highlights when pressed:
  * **A**: Emerald Green
  * **B**: Crimson Red
  * **X**: Cyan Blue
  * **Y**: Amber Yellow
  * **L / R / ZL / ZR / L3 / R3**: Electric Violet

### E. D-Pad Hat Switch
* Interactive cross layout: `▲`, `▼`, `◀`, `▶`.
* Highlights individual and diagonal presses.
* Displays raw hexadecimal value (e.g., `0x01` for UP, `0x02` for DOWN, `0x04` for RIGHT, `0x08` for LEFT).

### F. Special / Raw Inputs
* Displays live raw register values:
  * `buttons()` (e.g. `0x0000`)
  * `miscButtons()` (e.g. `0x0000`)
  * `dpad()` (e.g. `0x00`)
* Automatic bit decoding tags for special buttons:
  * `HOME`
  * `CAPTURE / SHARE`
  * `+` (START)
  * `-` (SELECT)
  * `TURBO`
  * `SYNC`

### G. Raw Data Register Table
* Monospace telemetry grid listing all 9 raw parameters in real-time.

### H. Input Activity Monitor
* Rolling chronological event log capturing button presses, stick moves, trigger changes, and D-pad actions with millisecond-precision timestamps.
* Includes a **CLEAR LOG** button.

---

## 7. Performance & Memory Optimizations

1. **Non-Blocking Architecture**:
   * Uses `millis()` intervals for telemetry transmission without `delay()`.
2. **Zero Dynamic Allocation**:
   * JSON telemetry is formatted using a fixed static buffer (`char jsonBuffer[384]`) with `snprintf()`, completely eliminating heap fragmentation.
3. **100% Offline UI in PROGMEM**:
   * HTML, CSS, JavaScript, and SVG icons are stored in Flash memory (`PROGMEM`), requiring no SD card, SPIFFS/LittleFS upload, or internet connection.
