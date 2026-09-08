#ifndef SPIDER_CONFIG_H
#define SPIDER_CONFIG_H

#include <Arduino.h>

// =============================================================================
// OPERATION MODE & OPTIONAL WEB DASHBOARD
// =============================================================================
// Set to 1 to enable concurrent Wi-Fi AP & Live Web Diagnostic Dashboard
// Set to 0 for pure low-power Bluetooth Gamepad Bridge mode
#define ENABLE_WEB_DASHBOARD     1

// =============================================================================
// ROBOT HARDWARE SERIAL COMMUNICATION (UART2 -> ARDUINO NANO)
// =============================================================================
#define ROBOT_TX_PIN             17      // ESP32 GPIO 17 -> Arduino Nano Pin 0 (RX)
#define ROBOT_RX_PIN             16      // ESP32 GPIO 16 <- Arduino Nano Pin 1 (TX)
#define ROBOT_BAUD               9600    // Nano kinematics communication baud rate

// USB Debug Serial
#define DEBUG_BAUD_RATE          115200

// =============================================================================
// GAMEPAD ANALOG THRESHOLDS & DEADZONES
// =============================================================================
// Analog stick range is approximately -512 to +512
#define STICK_DEADZONE           140     // Ignores center stick noise/drift
#define TRIGGER_THRESHOLD        350     // Analog trigger pull threshold (0..1020)

// Heartbeat interval (ms) to keep gait alive if stick held continuously
#define HEARTBEAT_INTERVAL_MS    1200

// Web telemetry broadcast rate (33 Hz / 30ms)
#define TELEMETRY_INTERVAL_MS    30

// =============================================================================
// WI-FI CONFIGURATION (IF WEB DASHBOARD ENABLED)
// =============================================================================
#define WIFI_SSID                "SpiderRobot"
#define WIFI_PASSWORD            "12345678"

#define AP_SSID                  "Spider-Controller"
#define AP_PASSWORD              "12345678"
#define AP_CHANNEL               1
#define AP_MAX_CLIENTS           4

#define HTTP_PORT                80
#define WS_PORT                  81

// =============================================================================
// CONTROLLER STATE DATA MODEL
// =============================================================================
struct GamepadState {
    bool connected;
    int index;
    char modelName[32];
    
    // Analog Sticks (-512 to +512)
    int16_t axisX;
    int16_t axisY;
    int16_t axisRX;
    int16_t axisRY;
    
    // Analog Triggers (0 to 1020)
    int16_t brake;      // ZL
    int16_t throttle;   // ZR
    
    // Digital Buttons
    bool a;
    bool b;
    bool x;
    bool y;
    bool l1;    // L
    bool r1;    // R
    bool l2;    // ZL button state
    bool r2;    // ZR button state
    bool thumbL; // L3
    bool thumbR; // R3
    
    // D-Pad (UP: 0x01, DOWN: 0x02, RIGHT: 0x04, LEFT: 0x08)
    uint8_t dpad;
    
    // Raw bitmasks
    uint16_t buttons;
    uint16_t miscButtons;
};

#endif // SPIDER_CONFIG_H
