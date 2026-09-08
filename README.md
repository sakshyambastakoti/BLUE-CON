# COCO Spider Robot

COCO is a 3-in-1 Arduino spider robot project built around a quadruped chassis with 12 servo motors. It supports three core operating modes: repetitive motion, obstacle avoidance, and Bluetooth control. The repository also includes ESP32-based web controller sketches for users who want Wi-Fi control from a phone or browser.

## Project Overview

This project is designed for makers who want a realistic walking robot that is still approachable to build at home. The robot uses 3D-printed parts, a Nano-based controller setup, a servo expansion shield, and a small set of widely available modules. The firmware is organized so you can build the robot in stages: first align the servos, then test the basic motions, then move on to obstacle avoidance or Bluetooth control.

### What the robot can do

- Walk forward and backward using smooth, timed leg motion
- Turn left and right in place
- Stand, sit, wave, shake hands, and dance
- Detect obstacles and change direction automatically
- Respond to Bluetooth commands from an Android app
- Optionally serve a mobile-friendly web interface through the ESP32 controller sketches

## Repository Contents

- `COCO Code/basic postion.ino` - centers all servos at the neutral position for assembly
- `COCO Code/repetitive motion.ino` - demo sketch for repeated walking and gesture sequences
- `COCO Code/obstcal avoding.ino` - autonomous obstacle avoidance behavior
- `COCO Code/Bluetooth-controlling_spider_robot_Robot_Lk.ino` - Bluetooth control sketch
- `COCO Code/ESP32_CAM_Controller/ESP32_CAM_Controller.ino` - ESP32-CAM web controller
- `COCO Code/ESP32_Web_Control/ESP32_Web_Control.ino` - ESP32 web controller variant
- `Spider_Gamepad_Controller/ESP32_Spider_Gamepad_Bridge/` - ESP32 Bluepad32 Bluetooth Gamepad Bridge
- `Spider_Gamepad_Controller/Spider_Robot_Nano_Firmware/` - High-performance responsive Arduino Nano Spider firmware
- `Spider_Gamepad_Controller/README.md` - Complete Gamepad wiring, pairing, and controls guide

## Required Components

| Component | Description | Quantity |
| --- | --- | ---: |
| 3D-printed spider robot parts | Body, legs, connectors, and brackets | 1 set |
| SG90 servo motors | 180-degree micro servos | 12 |
| Arduino Nano board | Main controller for the 3-in-1 robot | 1 |
| Nano IO Expansion Shield | Simplifies servo and module wiring | 1 |
| 2-cell 18650 battery holder | Battery holder for portable power | 1 |
| 18650 Li-ion batteries | 3.7V cells | 2 |
| LM2596 buck converter | Step-down power module for servos | 1 |
| HC-SR04 ultrasonic sensor | Obstacle detection | 1 |
| HC-05 or HC-06 Bluetooth module | Wireless phone control | 1 |
| Jumper wires | Female-to-female and general wiring | As needed |
| M3 screws and nuts | Mechanical assembly hardware | 4 sets |
| Glue gun and soldering tools | Assembly and wiring support | As needed |

## 3D Printed Parts

Use a layer height of 0.2 mm and around 15% infill for a good balance of strength and print time. If you do not have a printer, use a local or online printing service.

| STL File | Quantity |
| --- | ---: |
| `body_d.stl` | 1 |
| `body_u.stl` | 1 |
| `coxa_l.stl` | 2 |
| `coxa_r.stl` | 2 |
| `tibia_l.stl` | 2 |
| `tibia_r.stl` | 2 |
| `femur_1.stl` | 4 |
| `s_hold.stl` | 8 |

## Assembly Flow

### 1. Print and prepare the parts

Print all chassis and leg parts before installing servos. Clean any support material and test-fit each joint so the leg movement is smooth.

### 2. Assemble the legs

Follow the reference assembly images and install the servo motors into the leg sections one by one. Keep the servo horns loose at first so you can calibrate them at the neutral position later.

### 3. Align the servos

Before attaching the horns permanently, upload the centering sketch and power the robot. All servos should move to the midpoint, usually 90 degrees. While the servos are still powered, place the horns so each leg is aligned correctly, then secure them with screws.

### 4. Wire the electronics

Use the Nano IO Expansion Shield to keep the wiring neat. Connect the servo power rail to the LM2596 output, and always share a common ground between the Arduino, servo power supply, Bluetooth module, and ultrasonic sensor.

## Wiring Notes

### Servo pin mapping

The firmware uses 12 servo channels arranged as four legs with three joints per leg:

```cpp
const int servo_pin[4][3] = { {3, 4, 2}, {6, 7, 5}, {9, 8, 10}, {12, 11, 13} };
```

### Sensor and communication pins

- HC-SR04 trigger: A5
- HC-SR04 echo: A4
- OLED I2C SDA: A4
- OLED I2C SCL: A5
- Bluetooth module TX/RX: Arduino serial pins 0 and 1

## Initial Servo Alignment

The first calibration step is to center every servo before mounting the horn arms.

1. Upload `basic postion.ino`.
2. Power the servo rail so the servos can move and hold position.
3. Wait until all servos settle at the center point.
4. Mount the horns so the leg geometry matches the neutral pose shown in the assembly images.

This step is important. If the horns are mounted off-center, later walking motions will be uneven or mirrored incorrectly.

## Operating Modes

### 1. Basic repetitive movement

This sketch runs a demonstration sequence in a loop:

1. Stand up
2. Move forward for five steps
3. Move backward for five steps
4. Turn right
5. Turn left
6. Wave
7. Shake hands
8. Dance
9. Sit down

Use this mode when you want to verify the gait, servo alignment, and body balance.

### 2. Obstacle avoiding mode

This mode uses the HC-SR04 sensor to watch for obstacles ahead. When an obstacle is detected, the robot stops, steps back, and turns randomly left or right before continuing forward.

Typical behavior:

- Move forward while the path is clear
- Reverse a few steps if an object is too close
- Choose a new direction and continue

### 3. Bluetooth control mode

This mode lets you drive the robot with an Android Bluetooth app. It is best for manual control, testing, and demos.

#### Bluetooth command reference

| Command | Action |
| --- | --- |
| `F` | Walk forward |
| `B` | Walk backward |
| `L` | Turn left |
| `R` | Turn right |
| `U` | Stand up |
| `X` | Sit down |
| `D` | Body dance |
| `S` | Stop or idle |

## Arduino Code Structure

The main firmware pattern used across the sketches is consistent:

- `Servo.h` is used to drive the 12 servo motors
- `FlexiTimer2` is used to refresh motion smoothly at a regular interval
- `NewPing` is used for ultrasonic sensing in obstacle-avoidance mode
- `Adafruit_SSD1306` and `Adafruit_GFX` are used for the OLED display in Bluetooth mode

The motion logic is based on inverse kinematics, which converts target foot coordinates into servo angles. This makes the walking motion smoother and more realistic than simple on/off servo jumps.

## Upload Instructions

### Basic repetitive motion

1. Open `repetitive motion.ino` in the Arduino IDE.
2. Install the `FlexiTimer2` library using Library Manager or the ZIP file provided in the repository.
3. Select the correct board and COM port.
4. Upload the sketch and test the sequence.

### Obstacle avoidance

1. Open `obstcal avoding.ino`.
2. Install `FlexiTimer2` and `NewPing`.
3. Select the board and port.
4. Upload the sketch and verify the sensor reading range.

### Bluetooth control

1. Open `Bluetooth-controlling_spider_robot_Robot_Lk.ino`.
2. Install `FlexiTimer2`, `Adafruit SSD1306`, and `Adafruit GFX`.
3. Pair the HC-05 or HC-06 module with your phone.
4. Use a Bluetooth terminal or the provided Android app to send commands.

## ESP32 Web Controller

If you want Wi-Fi based control instead of Bluetooth, this repository also includes ESP32 controller sketches. These sketches create a Wi-Fi access point and expose a browser-based control page that can forward commands to the robot controller.

That setup is useful when you want:

- Phone control without pairing Bluetooth
- A larger on-screen control pad
- A cleaner browser interface for demos

## OTA Firmware Update (ESP32 Web Controller)

The ESP32 Web Controller includes an Over-The-Air (OTA) update feature built directly into the web interface. This allows you to flash new code wirelessly without plugging the ESP32 into your computer via a USB cable.

### How to use OTA Update:

1. Connect your device (phone or PC) to the **SpiderRobot** Wi-Fi network (Password: `12345678`).
2. Open your web browser and navigate to `http://192.168.4.1/`.
3. In the Tactical Interface under the **[ ACTIONS & PROTOCOLS ]** -> **UTILITIES** section, click the **SYS UPDATE** button.
4. You will be redirected to the Firmware Update page.
5. In the Arduino IDE, open your updated sketch and go to **Sketch** -> **Export compiled Binary** to generate a `.bin` file.
6. On the web update page, click **Choose File** and select your compiled `.bin` file.
7. Click **UPLOAD & FLASH**. 
8. The ESP32 will flash the new firmware wirelessly and automatically reboot when finished!

## Troubleshooting

- If the robot jitters or moves in the wrong direction, recheck servo horn alignment before changing the code.
- If the robot resets when servos move, the power supply is not strong enough for the servo load.
- If the ultrasonic sensor gives unstable readings, verify the trigger and echo pins and test at a short range first.
- If Bluetooth commands do not work, confirm the app is connected to the correct module and the baud rate matches the sketch.
- If upload fails, remove the Bluetooth module or disconnect serial wiring temporarily so it does not interfere with programming.

## Safety Notes

- Do not power all servos directly from the Arduino board.
- Keep hands clear of the legs during testing.
- Test the robot on a stand or raised surface during first power-up.
- Use a regulated supply and verify polarity before connecting the battery pack.

## Conclusion

COCO is a strong project for learning robotics, servo control, inverse kinematics, wireless communication, and embedded system integration. With the three operating modes, the robot can be used as a motion demo platform, an obstacle-avoiding robot, or a Bluetooth-controlled quadruped.

## Credits

Created by Skashyam Bastakoti. The repository content and tutorials are associated with the Robot LK project.