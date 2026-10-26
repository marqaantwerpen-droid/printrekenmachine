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

// ---------- Frame / beengeometrie (mm) ----------
// 1 = het veelgebruikte "Quadruped Spider Robot (SG90, Arduino Nano)"-frame
//     (o.a. PCBWay, regishsu / DIY GUY Chris), 0 = de eigen SPIN-onderdelen in cad/.
// Meet je eigen print na en pas de waarden aan; zie README "Eigen frame".
#define FRAME_SPIDER_SG90 1

#if FRAME_SPIDER_SG90
constexpr float COXA_LEN  = 27.5f;    // heup-as tot dij-as          (length_c)
constexpr float FEMUR_LEN = 55.0f;    // dij-as tot knie-as          (length_a)
constexpr float TIBIA_LEN = 77.5f;    // knie-as tot voetpunt        (length_b)
constexpr float HIP_SPACING = 71.0f;  // afstand tussen heupassen    (length_side)
// Positie van elke heup-as (x vooruit, y links) — vierkant van HIP_SPACING
constexpr float LEG_HIP_X[NUM_LEGS] = { HIP_SPACING / 2,  HIP_SPACING / 2, -HIP_SPACING / 2, -HIP_SPACING / 2 };
constexpr float LEG_HIP_Y[NUM_LEGS] = { HIP_SPACING / 2, -HIP_SPACING / 2,  HIP_SPACING / 2, -HIP_SPACING / 2 };
// Richting waarin de heup wijst als de heupservo op 90° staat (CCW, 0 = vooruit)
constexpr float LEG_NEUTRAL_DEG[NUM_LEGS] = { 90.0f, -90.0f, 90.0f, -90.0f };
// Richting van de voet in rusthouding: iets naar voren/achteren gespreid
constexpr float LEG_FOOT_DEG[NUM_LEGS] = { 60.0f, -60.0f, 120.0f, -120.0f };
constexpr float STAND_REACH  = 70.0f; // horizontale afstand heup -> voet
constexpr float STAND_HEIGHT = 55.0f; // dij-as boven de grond
constexpr float REST_HEIGHT  = 30.0f; // buik rust op de grond
constexpr float MIN_WALK_HEIGHT = 50.0f;
#else
constexpr float COXA_LEN  = 30.0f;
constexpr float FEMUR_LEN = 50.0f;
constexpr float TIBIA_LEN = 70.0f;
constexpr float BODY_HALF = 55.0f;    // centrum -> heup-as, op de diagonalen
constexpr float LEG_HIP_X[NUM_LEGS] = { BODY_HALF * 0.7071f,  BODY_HALF * 0.7071f, -BODY_HALF * 0.7071f, -BODY_HALF * 0.7071f };
constexpr float LEG_HIP_Y[NUM_LEGS] = { BODY_HALF * 0.7071f, -BODY_HALF * 0.7071f,  BODY_HALF * 0.7071f, -BODY_HALF * 0.7071f };
constexpr float LEG_NEUTRAL_DEG[NUM_LEGS] = { 45.0f, -45.0f, 135.0f, -135.0f };
constexpr float LEG_FOOT_DEG[NUM_LEGS]    = { 45.0f, -45.0f, 135.0f, -135.0f };
constexpr float STAND_REACH  = 95.0f;
constexpr float STAND_HEIGHT = 70.0f; // buik ~25 mm vrij
constexpr float REST_HEIGHT  = 38.0f;
constexpr float MIN_WALK_HEIGHT = 50.0f;
#endif

// Bewegingsveiligheid
constexpr uint32_t MAX_WALK_STEPS = 40;
constexpr uint16_t MIN_FRAME_MS   = 40;
