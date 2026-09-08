# Wireless Game Controller System for COCO Spider Robot

Control your **12-Servo COCO Spider Robot** using any wireless Bluetooth game controller (**DATA FROG S80, PlayStation DualShock 4, DualSense, Xbox Wireless, Nintendo Switch Pro, or 8BitDo**) with zero perceptible latency!

---

## 1. System Overview

The system uses an **ESP32 Classic (ESP32-WROOM-32)** as an ultra-fast wireless gamepad receiver and coprocessor running **Bluepad32**. The ESP32 connects directly to the game controller over Bluetooth Classic HID, translates analog sticks and button presses into kinematic gait commands, and sends them via Hardware UART to the **Arduino Nano** controlling the 12 SG90 servos.

```
 ┌────────────────────────────────────────────────────────┐
 │      DATA FROG S80 / Wireless Bluetooth Gamepad        │
 └───────────────────────────┬────────────────────────────┘
                             │ Bluetooth Classic HID
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │             ESP32 Classic (WROOM-32)                   │
 │   - Bluepad32 Bluetooth Gamepad Stack                  │
 │   - 20% Radial Deadzone & Direction Filtering          │
 │   - Edge-Triggered Button Dispatch                     │
 │   - Dual Haptic Vibration Feedback                     │
 │   - HardwareSerial2 (TX=GPIO 17, RX=GPIO 16)           │
 └───────────────────────────┬────────────────────────────┘
                             │ Serial UART @ 9600 Baud
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │           Arduino Nano + IO Expansion Shield           │
 │   - High-Performance Inverse Kinematics Engine         │
 │   - Instant Boot (< 300ms ready time)                  │
 │   - Responsive Interrupt-Driven Gait Switching         │
 │   - Graceful Auto-Grounding on Stop                    │
 │   - 12x SG90 Servo PWM Channels (FlexiTimer2)          │
 └────────────────────────────────────────────────────────┘
```

---

## 2. Gamepad Controls Cheat-Sheet

| Gamepad Input | Spider Robot Action | Serial Command | Notes |
|:---|:---|:---:|:---|
| **Left Stick UP** / **D-Pad UP** | **Walk Forward** | `'F'` | Continuous forward gait while held |
| **Left Stick DOWN** / **D-Pad DOWN** | **Walk Backward** | `'B'` | Continuous backward gait while held |
| **Left Stick LEFT** / **D-Pad LEFT** | **Turn Left** | `'L'` | Smooth spot rotation to the left |
| **Left Stick RIGHT** / **D-Pad RIGHT** | **Turn Right** | `'R'` | Smooth spot rotation to the right |
| **Stick Center / Release** | **Stop & Auto-Ground** | `'S'` | Gracefully brings all 4 feet to ground |
| **Button A** | **Hand Shake** | `'U'` | Front leg handshake gesture |
| **Button B** | **Hand Wave** | `'W'` | Friendly waving greeting gesture |
| **Button X** | **Basic Standing Stance** | `'P'` | Default standing posture |
| **Button Y** | **Spider Combat Stance** | `'Q'` | Low, wide, aggressive crouching stance |
| **Shoulder L1 (LB)** | **Body Dance** | `'V'` | Rhythmic body swaying dance |
| **Shoulder R1 (RB)** | **Eye LED Blink** | `'K'` | Blinks eye LEDs 5 times |
| **Trigger L2 (ZL)** | **Eye LED OFF** | `'X'` | Turns front eye LEDs off |
| **Trigger R2 (ZR)** | **Eye LED ON** | `'O'` | Turns front eye LEDs on |
| **Right Stick Click (R3)** / **HOME** | **EMERGENCY STOP** | `'S'` | Immediate motion freeze + heavy rumble |
| **Left Stick Click (L3)** | **Stance Toggle** | `'Q'` | Quick drop into spider stance |
| **START / PLUS (+)** | **Start Dance** | `'V'` | Triggers dance sequence |
| **SELECT / MINUS (-)** | **Reset Stance** | `'P'` | Resets to standing posture |

---

## 3. Hardware Wiring Diagram

Connect the ESP32 to the Arduino Nano IO shield using jumper wires:

| ESP32 Pin | Arduino Nano Pin | Description | Wire Color (Typical) |
|:---|:---|:---|:---|
| **GPIO 17 (TX2)** | **Pin 0 (RX)** | Serial commands from ESP32 to Nano | Yellow / Orange |
| **GPIO 16 (RX2)** | **Pin 1 (TX)** | Telemetry return line (optional) | Green |
| **GND** | **GND** | **Common Ground (MANDATORY)** | Black / Brown |
| **VIN / 5V** | **5V Power Rail** | Powered from LM2596 buck converter (5V) | Red |

### Servo Wiring (on Arduino Nano IO Shield)
* **Leg 0 (Front Right)**: Coxa = Pin 3, Femur = Pin 4, Tibia = Pin 2
* **Leg 1 (Back Right)**:  Coxa = Pin 6, Femur = Pin 7, Tibia = Pin 5
* **Leg 2 (Front Left)**:  Coxa = Pin 9, Femur = Pin 8, Tibia = Pin 10
* **Leg 3 (Back Left)**:   Coxa = Pin 12, Femur = Pin 11, Tibia = Pin 13
* **Status LEDs**: Pos = A0, Neg = A1
* **OLED Display (Optional)**: SDA = A4, SCL = A5

> [!IMPORTANT]
> **CRITICAL TIP FOR UPLOADING CODE TO ARDUINO NANO:**
> The Arduino Nano's onboard USB-to-Serial chip uses Pin 0 (RX) and Pin 1 (TX).
> **Unplug the wire going into Nano Pin 0 (RX) before clicking "Upload" in Arduino IDE.**
> Once the upload completes, plug the wire from ESP32 GPIO 17 back into Nano Pin 0.

---

## 4. Software Installation & Upload Guide

### Step 1: Install Required Libraries in Arduino IDE
Open Arduino IDE and install the following libraries via **Tools → Manage Libraries**:
1. **Bluepad32** by *Ricardo Quesada* (for ESP32)
2. **WebSockets** by *Markus Sattler* (for ESP32 optional web dashboard)
3. **FlexiTimer2** (available in your `library/` folder or via Library Manager for Arduino Nano)
4. **Adafruit SSD1306** & **Adafruit GFX** (for optional OLED display)

---

### Step 2: Upload Arduino Nano Firmware
1. Open [`Spider_Robot_Nano_Firmware.ino`](file:///d:/BLUE-CON/Spider_Gamepad_Controller/Spider_Robot_Nano_Firmware/Spider_Robot_Nano_Firmware.ino) in Arduino IDE.
2. Select **Tools → Board → Arduino AVR Boards → Arduino Nano**.
3. Select Processor: **ATmega328P** (or *ATmega328P (Old Bootloader)* depending on your Nano clone).
4. Select the correct COM Port.
5. Ensure **Pin 0 (RX)** is disconnected from the ESP32.
6. Click **Upload**.
7. Once uploaded, reconnect ESP32 GPIO 17 to Nano Pin 0.

---

### Step 3: Upload ESP32 Gamepad Bridge Firmware
1. Open [`ESP32_Spider_Gamepad_Bridge.ino`](file:///d:/BLUE-CON/Spider_Gamepad_Controller/ESP32_Spider_Gamepad_Bridge/ESP32_Spider_Gamepad_Bridge.ino) in Arduino IDE.
2. Select **Tools → Board → esp32 → ESP32 Dev Module** (or your specific ESP32 classic board).
3. Set Upload Speed: `921600` (or `115200`).
4. Click **Upload**.
5. Open the Serial Monitor at **115200 baud**.

---

## 5. Pairing the DATA FROG S80 Gamepad

1. Power on the robot and ESP32.
2. Put the **DATA FROG S80** controller into Bluetooth pairing mode:
   * **Nintendo Switch Mode (Recommended for Bluepad32)**:
     Press and hold **SYNC** button on top until the 4 LEDs flash in a running cycle.
     *(Or press **HOME + Y**)*
   * **Android / Standard HID Mode**:
     Press and hold **HOME + X** (or **HOME + B**) until the LED flashes rapidly.
3. The ESP32 running Bluepad32 will automatically discover, pair, and connect with the controller within 3–5 seconds!
4. **Feedback**:
   * The controller will vibrate with a welcoming double-rumble!
   * The controller LED turns Cyan/Blue.
   * The ESP32 Serial Monitor will print: `[GAMEPAD CONNECTED] Model: DATA FROG S80`.
5. Move the Left Analog Stick or D-Pad forward — your spider robot will immediately walk!

---

## 6. Optional Live Cyberpunk Web Dashboard

If you want to view real-time analog stick graphs, battery readings, and raw diagnostics on your smartphone or PC while driving with the controller:
1. Connect your phone or laptop Wi-Fi to the network:
   * **SSID**: `Spider-Controller`
   * **Password**: `12345678`
2. Open your web browser and navigate to:
   * `http://192.168.4.1/`
3. You will see the real-time HUD with animated joysticks, trigger bars, tactile button matrix, and live command monitor!

---

## 7. Troubleshooting

* **Robot does not respond to gamepad:**
  1. Verify the common ground wire between ESP32 `GND` and Arduino Nano `GND`.
  2. Confirm ESP32 GPIO 17 is connected to Arduino Nano Pin 0 (RX).
  3. Verify baud rate on both devices is set to 9600 baud.
* **Nano fails to upload sketch via USB:**
  * Unplug the wire from Pin 0 (RX) during upload. Reconnect it after upload succeeds.
* **Robot resets when walking:**
  * SG90 servos draw up to 2.5A to 3.5A peak under load. Ensure your 2x 18650 batteries are charged and the LM2596 buck converter is adjusted to 5.0V–6.0V output.
* **Clearing Old Bluetooth Pairings:**
  * In `ESP32_Spider_Gamepad_Bridge.ino`, uncomment `BP32.forgetBluetoothKeys();` in `setup()` once, upload, and comment it out again to clear paired devices.
