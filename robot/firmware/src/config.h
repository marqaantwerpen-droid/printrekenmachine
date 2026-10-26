// SPIN — configuratie: pinnen, beengeometrie en limieten.
#pragma once
#include <Arduino.h>

#define ROBOT_NAME "spin"          // bereikbaar als http://spin.local

// ---------- Servo's (14x MG90S) ----------
// Index:  0..2  = voor-links  (heup, dij, scheen)
//         3..5  = voor-rechts
//         6..8  = achter-links
//         9..11 = achter-rechts
//         12    = hoofd pan,  13 = hoofd tilt
constexpr uint8_t NUM_SERVOS = 14;
constexpr uint8_t NUM_LEGS = 4;

constexpr uint8_t SERVO_PINS[NUM_SERVOS] = {
  13, 14, 27,   // FL
  16, 17, 18,   // FR
  25, 26, 33,   // RL
  19, 23, 32,   // RR
  4,  5         // hoofd pan / tilt
};

// Standaard draairichting per servo (+1 / -1). Gespiegelde poten staan op -1.
// Aanpasbaar zonder flashen via het commando "calibrate" (wordt bewaard).
constexpr int8_t SERVO_DIR_DEFAULT[NUM_SERVOS] = {
  1,  1, -1,   // FL  (heup, dij, scheen)
  1, -1,  1,   // FR
  1,  1, -1,   // RL
  1, -1,  1,   // RR
  1,  1        // hoofd
};

constexpr uint8_t SERVO_MIN_DEG = 10;   // harde veiligheidsgrenzen
constexpr uint8_t SERVO_MAX_DEG = 170;

// ---------- Overige randapparatuur ----------
constexpr uint8_t PIN_I2C_SDA = 21;    // OLED 0.91" (SSD1306 128x32)
constexpr uint8_t PIN_I2C_SCL = 22;
constexpr uint8_t OLED_ADDR   = 0x3C;
constexpr uint8_t PIN_LEDS    = 15;    // WS2812B data (via 330 Ω)
constexpr uint8_t NUM_LEDS    = 8;
constexpr uint8_t PIN_HALL    = 34;    // hall-sensor (alleen-input pin)
constexpr bool    HALL_ACTIVE_LOW = true;   // KY-003/A3144: LOW = magneet
constexpr uint8_t PIN_VBAT    = 35;    // optioneel: accuspanning via 100k/47k deler
constexpr float   VBAT_DIVIDER = (100.0f + 47.0f) / 47.0f;

// ---------- Beengeometrie (mm) — klopt met de CAD-onderdelen ----------
constexpr float COXA_LEN  = 30.0f;    // heup-as tot dij-as
constexpr float FEMUR_LEN = 50.0f;    // dij-as tot knie-as
constexpr float TIBIA_LEN = 70.0f;    // knie-as tot voetpunt

// Montagehoek van elke heup t.o.v. de voorkant (graden, CCW, x = vooruit)
constexpr float LEG_MOUNT_DEG[NUM_LEGS] = { 45.0f, -45.0f, 135.0f, -135.0f };
constexpr float BODY_HALF = 55.0f;    // afstand centrum -> heup-as (mm)

// Standaard stand
constexpr float STAND_REACH  = 95.0f; // horizontale afstand heup -> voet
constexpr float STAND_HEIGHT = 70.0f; // dij-as boven de grond (buik ~25 mm vrij)

// Bewegingsveiligheid
constexpr uint32_t MAX_WALK_STEPS = 40;
constexpr uint16_t MIN_FRAME_MS   = 40;
constexpr float    REST_HEIGHT    = 38.0f;  // buik rust op de grond
constexpr float    MIN_WALK_HEIGHT = 50.0f;
