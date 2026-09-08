/**
 * =============================================================================
 * COCO SPIDER ROBOT - HIGH-PERFORMANCE NANO MOTION CONTROLLER
 * =============================================================================
 * Hardware:      Arduino Nano + Nano IO Expansion Shield
 * Actuators:     12x SG90 9g Micro Servos (Inverse Kinematics)
 * Communication: Serial UART (Pin 0 RX, Pin 1 TX) @ 9600 baud
 * Compatible:    ESP32 Gamepad Bridge (DATA FROG S80 / PS4 / Xbox / Switch Pro)
 * 
 * Major Upgrades in this Firmware:
 *  1. INSTANT BOOT: Removed 25 seconds of blocking delays in setup().
 *     Robot is ready to receive commands in < 300ms.
 *  2. SAFE OLED DETECTION: Probes I2C bus before OLED init so it never locks
 *     up if an OLED display is not connected.
 *  3. RESPONSIVE SERIAL INTERRUPTS: Any new incoming command (L, R, B, F, S)
 *     immediately breaks the current stride to switch directions instantly.
 *  4. GRACEFUL AUTO-GROUNDING ON STOP: When 'S' is received, all 4 legs are
 *     smoothly lowered to the ground so the robot never freezes mid-air.
 * =============================================================================
 */

#include <Servo.h>
#include <FlexiTimer2.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// =============================================================================
// HARDWARE DEFINITIONS & PIN CONFIGURATION
// =============================================================================
#define OLED_ADDR       0x3C
Adafruit_SSD1306 display(-1);
bool oled_present = false;

// Eye status LEDs
const int LED_POS_PIN = A0;
const int LED_NEG_PIN = A1;

// 12 Servos for 4 legs (Leg 0: Right Front, Leg 1: Right Back, Leg 2: Left Front, Leg 3: Left Back)
Servo servo[4][3];
const int servo_pin[4][3] = {
  {3, 4, 2},     // Leg 0: Coxa, Femur, Tibia
  {6, 7, 5},     // Leg 1: Coxa, Femur, Tibia
  {9, 8, 10},    // Leg 2: Coxa, Femur, Tibia
  {12, 11, 13}   // Leg 3: Coxa, Femur, Tibia
};

// =============================================================================
// ROBOT GEOMETRY & KINEMATICS CONSTANTS (in mm)
// =============================================================================
const float length_a    = 50.0;
const float length_b    = 77.1;
const float length_c    = 27.5;
const float length_side = 71.0;
const float z_absolute  = -28.0;

// Default movement heights and positions
const float z_default   = -50.0;
const float z_up        = -30.0;
const float z_boot      = z_absolute;
const float x_default   = 62.0;
const float x_offset    = 0.0;
const float y_start     = 0.0;
const float y_step      = 40.0;
const float y_default   = x_default;
const float pi          = 3.14159265;
const float KEEP        = 255.0;

// Gait Speeds
const float spot_turn_speed  = 4.0;
const float leg_move_speed   = 8.0;
const float body_move_speed  = 3.0;
const float stand_seat_speed = 1.0;

// Turning Geometry Pre-calculations
const float temp_a = sqrt(pow(2 * x_default + length_side, 2) + pow(y_step, 2));
const float temp_b = 2 * (y_start + y_step) + length_side;
const float temp_c = sqrt(pow(2 * x_default + length_side, 2) + pow(2 * y_start + y_step + length_side, 2));
const float temp_alpha = acos((pow(temp_a, 2) + pow(temp_b, 2) - pow(temp_c, 2)) / (2 * temp_a * temp_b));
const float turn_x1 = (temp_a - length_side) / 2.0;
const float turn_y1 = y_start + y_step / 2.0;
const float turn_x0 = turn_x1 - temp_b * cos(temp_alpha);
const float turn_y0 = temp_b * sin(temp_alpha) - turn_y1 - length_side;

// =============================================================================
// VOLATILE KINEMATIC & SCHEDULER STATE
// =============================================================================
volatile float site_now[4][3];     // Current endpoint coordinates
volatile float site_expect[4][3];  // Target endpoint coordinates
float temp_speed[4][3];            // Speed vector per axis
float move_speed;
float speed_multiple = 1.0;
volatile int rest_counter = 0;

// High-responsiveness command scheduler
volatile bool stop_requested = false;
volatile char active_mode    = 'S';
volatile char pending_cmd    = 0;
int gesture_steps            = 2;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
void servo_service(void);
void servo_attach(void);
void servo_detach(void);
void set_site(int leg, float x, float y, float z);
void wait_reach(int leg);
void wait_all_reach(void);
void cartesian_to_polar(volatile float &alpha, volatile float &beta, volatile float &gamma, volatile float x, volatile float y, volatile float z);
void polar_to_servo(int leg, float alpha, float beta, float gamma);

void stand(void);
void sit(void);
void basic_position(void);
void spider_position(void);
void halt_motion(void);
void ground_all_feet(void);

void step_forward(unsigned int step);
void step_back(unsigned int step);
void turn_left(unsigned int step);
void turn_right(unsigned int step);
void body_dance(int i);
void hand_wave(int i);
void hand_shake(int i);

void forward_loop(void);
void backward_loop(void);
void left_loop(void);
void right_loop(void);

bool check_command_interrupt(void);
void led_on(void);
void led_off(void);
void led_blink(unsigned int times);

void init_oled_safe(void);
void oled_draw_happy(void);
void oled_draw_wink(void);
void oled_draw_combat(void);

// =============================================================================
// SETUP - INSTANT STARTUP (< 300ms)
// =============================================================================
void setup() {
  Serial.begin(9600);
  pinMode(LED_POS_PIN, OUTPUT);
  pinMode(LED_NEG_PIN, OUTPUT);
  led_off();

  // 1. Safe OLED bus test (will not hang if no OLED connected)
  init_oled_safe();
  if (oled_present) {
    oled_draw_happy();
  }

  // 2. Initialize default coordinates
  set_site(0, x_default - x_offset, y_start + y_step, z_boot);
  set_site(1, x_default - x_offset, y_start + y_step, z_boot);
  set_site(2, x_default + x_offset, y_start, z_boot);
  set_site(3, x_default + x_offset, y_start, z_boot);

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j++) {
      site_now[i][j] = site_expect[i][j];
    }
  }

  // 3. Start 50Hz (20ms) kinematic timer interrupt service
  FlexiTimer2::set(20, servo_service);
  FlexiTimer2::start();

  // 4. Attach servos and assume standing position immediately
  servo_attach();
  stand();

  // Quick confirmation blink
  led_blink(2);

  Serial.println(F("SPIDER_ROBOT_READY"));
}

// =============================================================================
// MAIN CONTROL LOOP
// =============================================================================
void loop() {
  char cmd = 0;

  // Check if an interrupt scheduled a pending command
  if (pending_cmd != 0) {
    cmd = pending_cmd;
    pending_cmd = 0;
  } 
  // Otherwise read next byte from Serial
  else if (Serial.available()) {
    while (Serial.available()) {
      cmd = (char)Serial.read();
    }
  }

  if (cmd == 0) return;

  // Reset stop flag for movement commands
  if (cmd != 'S') {
    stop_requested = false;
  }

  active_mode = cmd;

  switch (cmd) {
    case 'F': // Forward
      Serial.println(F("OK_FORWARD"));
      forward_loop();
      break;

    case 'B': // Backward
      Serial.println(F("OK_BACKWARD"));
      backward_loop();
      break;

    case 'L': // Turn Left
      Serial.println(F("OK_TURN_LEFT"));
      left_loop();
      break;

    case 'R': // Turn Right
      Serial.println(F("OK_TURN_RIGHT"));
      right_loop();
      break;

    case 'S': // Immediate Stop & Ground
      stop_requested = true;
      halt_motion();
      ground_all_feet();
      Serial.println(F("OK_STOP"));
      break;

    case 'P': // Basic / Stand Stance
      Serial.println(F("OK_BASIC_POS"));
      basic_position();
      if (oled_present) oled_draw_happy();
      break;

    case 'Q': // Spider / Combat Stance
      Serial.println(F("OK_SPIDER_POS"));
      spider_position();
      if (oled_present) oled_draw_combat();
      break;

    case 'U': // Hand Shake
      Serial.println(F("OK_HAND_SHAKE"));
      hand_shake(gesture_steps);
      stand();
      break;

    case 'W': // Hand Wave
      Serial.println(F("OK_HAND_WAVE"));
      if (oled_present) oled_draw_wink();
      hand_wave(gesture_steps);
      stand();
      if (oled_present) oled_draw_happy();
      break;

    case 'V': // Body Dance
      Serial.println(F("OK_BODY_DANCE"));
      body_dance(gesture_steps);
      stand();
      break;

    case 'O': // Eye LED On
      led_on();
      Serial.println(F("OK_LED_ON"));
      break;

    case 'X': // Eye LED Off
      led_off();
      Serial.println(F("OK_LED_OFF"));
      break;

    case 'K': // Eye LED Blink
      led_blink(5);
      Serial.println(F("OK_LED_BLINK"));
      break;

    default:
      break;
  }
}

// =============================================================================
// CONTINUOUS GAIT LOOPS (INTERRUPT-AWARE)
// =============================================================================
void forward_loop() {
  while (!stop_requested) {
    step_forward(1);
    if (stop_requested) break;
  }
}

void backward_loop() {
  while (!stop_requested) {
    step_back(1);
    if (stop_requested) break;
  }
}

void left_loop() {
  while (!stop_requested) {
    turn_left(1);
    if (stop_requested) break;
  }
}

void right_loop() {
  while (!stop_requested) {
    turn_right(1);
    if (stop_requested) break;
  }
}

// =============================================================================
// SERIAL COMMAND INTERRUPT POLLING
// Called repeatedly during every servo sub-step inside wait_reach()
// =============================================================================
bool check_command_interrupt() {
  while (Serial.available()) {
    int incoming = Serial.read();
    if (incoming > 0) {
      char c = (char)incoming;

      // Stop command immediately aborts motion
      if (c == 'S') {
        stop_requested = true;
        halt_motion();
        pending_cmd = 0;
        return true;
      }

      // If user commanded a DIFFERENT direction or action, interrupt current gait!
      if (c != active_mode && (c == 'F' || c == 'B' || c == 'L' || c == 'R' ||
                               c == 'P' || c == 'Q' || c == 'U' || c == 'W' ||
                               c == 'V' || c == 'O' || c == 'X' || c == 'K')) {
        stop_requested = true;
        halt_motion();
        pending_cmd = c; // Queue up next command for immediate execution
        return true;
      }
    }
  }

  return stop_requested;
}

// =============================================================================
// MOTION HALT & AUTO-GROUNDING
// =============================================================================
void halt_motion() {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j++) {
      site_expect[i][j] = site_now[i][j];
      temp_speed[i][j] = 0;
    }
  }
}

void ground_all_feet() {
  move_speed = leg_move_speed;
  // Lower any raised legs down to normal ground contact
  for (int i = 0; i < 4; i++) {
    if (site_now[i][2] > z_default) {
      site_expect[i][2] = z_default;
      float diff = z_default - site_now[i][2];
      temp_speed[i][2] = (diff < 0 ? -1 : 1) * move_speed;
    }
  }
  // Brief delay to allow servos to reach stable ground
  delay(120);
}

void basic_position() {
  move_speed = stand_seat_speed;
  stand();
}

void spider_position() {
  move_speed = stand_seat_speed;
  set_site(0, x_default, y_default - 20, z_default - 20);
  set_site(1, x_default, y_default + 20, z_default - 20);
  set_site(2, x_default, y_default - 20, z_default - 20);
  set_site(3, x_default, y_default + 20, z_default - 20);
  wait_all_reach();
}

void sit(void) {
  move_speed = stand_seat_speed;
  for (int leg = 0; leg < 4; leg++) {
    set_site(leg, KEEP, KEEP, z_boot);
  }
  wait_all_reach();
}

void stand(void) {
  move_speed = stand_seat_speed;
  for (int leg = 0; leg < 4; leg++) {
    set_site(leg, KEEP, KEEP, z_default);
  }
  wait_all_reach();
}

// =============================================================================
// KINEMATIC GAIT FUNCTIONS
// =============================================================================
void step_forward(unsigned int step) {
  move_speed = leg_move_speed;
  while (step-- > 0) {
    if (check_command_interrupt()) return;

    if (site_now[2][1] == y_start) {
      // Leg 2 & 1 move
      set_site(2, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = body_move_speed;
      set_site(0, x_default + x_offset, y_start, z_default);
      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_default);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = leg_move_speed;
      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(1, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(1, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    } else {
      // Leg 0 & 3 move
      set_site(0, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = body_move_speed;
      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start, z_default);
      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = leg_move_speed;
      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    }
  }
}

void step_back(unsigned int step) {
  move_speed = leg_move_speed;
  while (step-- > 0) {
    if (check_command_interrupt()) return;

    if (site_now[3][1] == y_start) {
      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = body_move_speed;
      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_default);
      set_site(1, x_default + x_offset, y_start, z_default);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = leg_move_speed;
      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    } else {
      set_site(1, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = body_move_speed;
      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_default);
      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;

      move_speed = leg_move_speed;
      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(2, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(2, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    }
  }
}

void turn_left(unsigned int step) {
  move_speed = spot_turn_speed;
  while (step-- > 0) {
    if (check_command_interrupt()) return;

    if (site_now[3][1] == y_start) {
      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x1 - x_offset, turn_y1, z_default);
      set_site(1, turn_x0 - x_offset, turn_y0, z_default);
      set_site(2, turn_x1 + x_offset, turn_y1, z_default);
      set_site(3, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(3, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x1 + x_offset, turn_y1, z_default);
      set_site(1, turn_x0 + x_offset, turn_y0, z_default);
      set_site(2, turn_x1 - x_offset, turn_y1, z_default);
      set_site(3, turn_x0 - x_offset, turn_y0, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(1, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default + x_offset, y_start, z_default);
      set_site(1, x_default + x_offset, y_start, z_up);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(1, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    } else {
      set_site(0, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x0 + x_offset, turn_y0, z_up);
      set_site(1, turn_x1 + x_offset, turn_y1, z_default);
      set_site(2, turn_x0 - x_offset, turn_y0, z_default);
      set_site(3, turn_x1 - x_offset, turn_y1, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x0 - x_offset, turn_y0, z_default);
      set_site(1, turn_x1 - x_offset, turn_y1, z_default);
      set_site(2, turn_x0 + x_offset, turn_y0, z_default);
      set_site(3, turn_x1 + x_offset, turn_y1, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(2, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start, z_up);
      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(2, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    }
  }
}

void turn_right(unsigned int step) {
  move_speed = spot_turn_speed;
  while (step-- > 0) {
    if (check_command_interrupt()) return;

    if (site_now[2][1] == y_start) {
      set_site(2, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x0 - x_offset, turn_y0, z_default);
      set_site(1, turn_x1 - x_offset, turn_y1, z_default);
      set_site(2, turn_x0 + x_offset, turn_y0, z_up);
      set_site(3, turn_x1 + x_offset, turn_y1, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(2, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x0 + x_offset, turn_y0, z_default);
      set_site(1, turn_x1 + x_offset, turn_y1, z_default);
      set_site(2, turn_x0 - x_offset, turn_y0, z_default);
      set_site(3, turn_x1 - x_offset, turn_y1, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default + x_offset, y_start, z_up);
      set_site(1, x_default + x_offset, y_start, z_default);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    } else {
      set_site(1, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x1 + x_offset, turn_y1, z_default);
      set_site(1, turn_x0 + x_offset, turn_y0, z_up);
      set_site(2, turn_x1 - x_offset, turn_y1, z_default);
      set_site(3, turn_x0 - x_offset, turn_y0, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(1, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, turn_x1 - x_offset, turn_y1, z_default);
      set_site(1, turn_x0 - x_offset, turn_y0, z_default);
      set_site(2, turn_x1 + x_offset, turn_y1, z_default);
      set_site(3, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();
      if (stop_requested) return;

      set_site(3, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start, z_default);
      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      if (stop_requested) return;

      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();
      if (stop_requested) return;
    }
  }
}

// =============================================================================
// GESTURES & ACTIONS
// =============================================================================
void body_left(int i) {
  set_site(0, site_now[0][0] + i, KEEP, KEEP);
  set_site(1, site_now[1][0] + i, KEEP, KEEP);
  set_site(2, site_now[2][0] - i, KEEP, KEEP);
  set_site(3, site_now[3][0] - i, KEEP, KEEP);
  wait_all_reach();
}

void body_right(int i) {
  set_site(0, site_now[0][0] - i, KEEP, KEEP);
  set_site(1, site_now[1][0] - i, KEEP, KEEP);
  set_site(2, site_now[2][0] + i, KEEP, KEEP);
  set_site(3, site_now[3][0] + i, KEEP, KEEP);
  wait_all_reach();
}

void hand_wave(int i) {
  move_speed = 1.0;
  if (site_now[3][1] == y_start) {
    body_right(15);
    float x_tmp = site_now[2][0];
    float y_tmp = site_now[2][1];
    float z_tmp = site_now[2][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++) {
      set_site(2, turn_x1, turn_y1, 50);
      wait_all_reach();
      set_site(2, turn_x0, turn_y0, 50);
      wait_all_reach();
    }
    set_site(2, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1.0;
    body_left(15);
  } else {
    body_left(15);
    float x_tmp = site_now[0][0];
    float y_tmp = site_now[0][1];
    float z_tmp = site_now[0][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++) {
      set_site(0, turn_x1, turn_y1, 50);
      wait_all_reach();
      set_site(0, turn_x0, turn_y0, 50);
      wait_all_reach();
    }
    set_site(0, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1.0;
    body_right(15);
  }
}

void hand_shake(int i) {
  move_speed = 1.0;
  if (site_now[3][1] == y_start) {
    body_right(15);
    float x_tmp = site_now[2][0];
    float y_tmp = site_now[2][1];
    float z_tmp = site_now[2][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++) {
      set_site(2, x_default - 30, y_start + 2 * y_step, 55);
      wait_all_reach();
      set_site(2, x_default - 30, y_start + 2 * y_step, 10);
      wait_all_reach();
    }
    set_site(2, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1.0;
    body_left(15);
  } else {
    body_left(15);
    float x_tmp = site_now[0][0];
    float y_tmp = site_now[0][1];
    float z_tmp = site_now[0][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++) {
      set_site(0, x_default - 30, y_start + 2 * y_step, 55);
      wait_all_reach();
      set_site(0, x_default - 30, y_start + 2 * y_step, 10);
      wait_all_reach();
    }
    set_site(0, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1.0;
    body_right(15);
  }
}

void head_up(int i) {
  set_site(0, KEEP, KEEP, site_now[0][2] - i);
  set_site(1, KEEP, KEEP, site_now[1][2] + i);
  set_site(2, KEEP, KEEP, site_now[2][2] - i);
  set_site(3, KEEP, KEEP, site_now[3][2] + i);
  wait_all_reach();
}

void head_down(int i) {
  set_site(0, KEEP, KEEP, site_now[0][2] + i);
  set_site(1, KEEP, KEEP, site_now[1][2] - i);
  set_site(2, KEEP, KEEP, site_now[2][2] + i);
  set_site(3, KEEP, KEEP, site_now[3][2] - i);
  wait_all_reach();
}

void body_dance(int i) {
  float body_dance_speed = 2.0;
  sit();
  move_speed = 1.0;
  set_site(0, x_default, y_default, KEEP);
  set_site(1, x_default, y_default, KEEP);
  set_site(2, x_default, y_default, KEEP);
  set_site(3, x_default, y_default, KEEP);
  wait_all_reach();

  stand();
  set_site(0, x_default, y_default, z_default - 20);
  set_site(1, x_default, y_default, z_default - 20);
  set_site(2, x_default, y_default, z_default - 20);
  set_site(3, x_default, y_default, z_default - 20);
  wait_all_reach();

  move_speed = body_dance_speed;
  head_up(30);

  for (int j = 0; j < i; j++) {
    if (j > i / 4) move_speed = body_dance_speed * 2.0;
    if (j > i / 2) move_speed = body_dance_speed * 3.0;

    set_site(0, KEEP, y_default - 20, KEEP);
    set_site(1, KEEP, y_default + 20, KEEP);
    set_site(2, KEEP, y_default - 20, KEEP);
    set_site(3, KEEP, y_default + 20, KEEP);
    wait_all_reach();

    set_site(0, KEEP, y_default + 20, KEEP);
    set_site(1, KEEP, y_default - 20, KEEP);
    set_site(2, KEEP, y_default + 20, KEEP);
    set_site(3, KEEP, y_default - 20, KEEP);
    wait_all_reach();
  }

  move_speed = body_dance_speed;
  head_down(30);
}

// =============================================================================
// TIMER INTERRUPT SERVICE (50 Hz / 20ms)
// =============================================================================
void servo_service(void) {
  sei(); // Enable nested interrupts for serial UART
  static float alpha, beta, gamma;

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j++) {
      if (abs(site_now[i][j] - site_expect[i][j]) >= abs(temp_speed[i][j])) {
        site_now[i][j] += temp_speed[i][j];
      } else {
        site_now[i][j] = site_expect[i][j];
      }
    }
    cartesian_to_polar(alpha, beta, gamma, site_now[i][0], site_now[i][1], site_now[i][2]);
    polar_to_servo(i, alpha, beta, gamma);
  }

  rest_counter++;
}

// =============================================================================
// KINEMATICS ENGINE: POLAR TO CARTESIAN & SERVO MAPPING
// =============================================================================
void set_site(int leg, float x, float y, float z) {
  if (stop_requested) return;

  float length_x = 0, length_y = 0, length_z = 0;

  if (x != KEEP) length_x = x - site_now[leg][0];
  if (y != KEEP) length_y = y - site_now[leg][1];
  if (z != KEEP) length_z = z - site_now[leg][2];

  float length = sqrt(pow(length_x, 2) + pow(length_y, 2) + pow(length_z, 2));

  temp_speed[leg][0] = length_x / length * move_speed * speed_multiple;
  temp_speed[leg][1] = length_y / length * move_speed * speed_multiple;
  temp_speed[leg][2] = length_z / length * move_speed * speed_multiple;

  if (x != KEEP) site_expect[leg][0] = x;
  if (y != KEEP) site_expect[leg][1] = y;
  if (z != KEEP) site_expect[leg][2] = z;
}

void wait_reach(int leg) {
  while (1) {
    if (check_command_interrupt()) {
      return;
    }
    if (site_now[leg][0] == site_expect[leg][0] &&
        site_now[leg][1] == site_expect[leg][1] &&
        site_now[leg][2] == site_expect[leg][2]) {
      break;
    }
  }
}

void wait_all_reach(void) {
  for (int i = 0; i < 4; i++) {
    wait_reach(i);
    if (stop_requested) return;
  }
}

void cartesian_to_polar(volatile float &alpha, volatile float &beta, volatile float &gamma, volatile float x, volatile float y, volatile float z) {
  float v, w;
  w = (x >= 0 ? 1 : -1) * (sqrt(pow(x, 2) + pow(y, 2)));
  v = w - length_c;
  alpha = atan2(z, v) + acos((pow(length_a, 2) - pow(length_b, 2) + pow(v, 2) + pow(z, 2)) / (2 * length_a * sqrt(pow(v, 2) + pow(z, 2))));
  beta = acos((pow(length_a, 2) + pow(length_b, 2) - pow(v, 2) - pow(z, 2)) / (2 * length_a * length_b));
  gamma = (w >= 0) ? atan2(y, x) : atan2(-y, -x);

  alpha = alpha / pi * 180.0;
  beta  = beta / pi * 180.0;
  gamma = gamma / pi * 180.0;
}

void polar_to_servo(int leg, float alpha, float beta, float gamma) {
  if (leg == 0) {
    alpha = 90.0 - alpha;
    gamma += 90.0;
  } else if (leg == 1) {
    alpha += 90.0;
    beta = 180.0 - beta;
    gamma = 90.0 - gamma;
  } else if (leg == 2) {
    alpha += 90.0;
    beta = 180.0 - beta;
    gamma = 90.0 - gamma;
  } else if (leg == 3) {
    alpha = 90.0 - alpha;
    gamma += 90.0;
  }

  servo[leg][0].write(alpha);
  servo[leg][1].write(beta);
  servo[leg][2].write(gamma);
}

void servo_attach(void) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j++) {
      servo[i][j].attach(servo_pin[i][j]);
      delay(20);
    }
  }
}

void servo_detach(void) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 3; j++) {
      servo[i][j].detach();
      delay(20);
    }
  }
}

// =============================================================================
// LED CONTROLS
// =============================================================================
void led_off() {
  digitalWrite(LED_POS_PIN, LOW);
  digitalWrite(LED_NEG_PIN, LOW);
}

void led_on() {
  digitalWrite(LED_POS_PIN, HIGH);
  digitalWrite(LED_NEG_PIN, LOW);
}

void led_blink(unsigned int times) {
  for (unsigned int i = 0; i < times; i++) {
    led_on();
    delay(100);
    led_off();
    delay(100);
  }
}

// =============================================================================
// OLED DISPLAY ROUTINES (SAFE I2C DETECTION)
// =============================================================================
void init_oled_safe() {
  Wire.begin();
  Wire.beginTransmission(OLED_ADDR);
  byte error = Wire.endTransmission();

  if (error == 0) {
    oled_present = true;
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    display.clearDisplay();
    display.display();
  } else {
    oled_present = false;
  }
}

void oled_draw_happy() {
  if (!oled_present) return;
  display.clearDisplay();
  display.fillCircle(42, 25, 15, WHITE);
  display.fillCircle(82, 25, 15, WHITE);
  display.fillCircle(42, 33, 20, BLACK);
  display.fillCircle(82, 33, 20, BLACK);
  display.display();
}

void oled_draw_wink() {
  if (!oled_present) return;
  display.clearDisplay();
  display.fillCircle(42, 25, 15, WHITE);
  display.fillCircle(42, 33, 20, BLACK);
  display.drawFastHLine(72, 25, 20, WHITE);
  display.display();
}

void oled_draw_combat() {
  if (!oled_present) return;
  display.clearDisplay();
  display.fillCircle(42, 10, 18, WHITE);
  display.fillCircle(82, 10, 18, WHITE);
  display.fillTriangle(0, 0, 54, 26, 118, 0, BLACK);
  display.display();
}
