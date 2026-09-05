#ifndef S80_CONFIG_H
#define S80_CONFIG_H

#include <Arduino.h>

// =============================================================================
// WI-FI ACCESS POINT CONFIGURATION
// =============================================================================
#define AP_SSID          "S80-DIAGNOSTIC"
#define AP_PASSWORD      "12345678"
#define AP_CHANNEL       1
#define AP_MAX_CLIENTS   4

// Network ports
#define HTTP_PORT        80
#define WS_PORT          81

// =============================================================================
// TELEMETRY & PERFORMANCE CONFIGURATION
// =============================================================================
// Target update rate: 30-40 Hz (25-33 ms interval)
#define TELEMETRY_INTERVAL_MS   30 

// Serial baud rate
#define SERIAL_BAUD_RATE        115200

// Analog deadzone for event detection
#define STICK_DEADZONE          40
#define TRIGGER_DEADZONE        15

// =============================================================================
// CONTROLLER STATE DATA STRUCTURE
// =============================================================================
struct GamepadState {
    bool connected;
    int index;
    char modelName[32];
    
    // Analog Sticks (approx -512 to +512)
    int16_t axisX;
    int16_t axisY;
    int16_t axisRX;
    int16_t axisRY;
    
    // Analog Triggers (0 to ~1020)
    int16_t brake;      // ZL
    int16_t throttle;   // ZR
    
    // Main Buttons (Boolean states)
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
    
    // D-Pad raw bitmask (UP: 0x01, DOWN: 0x02, RIGHT: 0x04, LEFT: 0x08)
    uint8_t dpad;
    
    // Raw bitmasks
    uint16_t buttons;       // ctl->buttons()
    uint16_t miscButtons;   // ctl->miscButtons()
};

#endif // S80_CONFIG_H
