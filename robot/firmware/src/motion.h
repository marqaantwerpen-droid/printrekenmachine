// Bewegingsmotor: een wachtrij van keyframes die vloeiend worden afgespeeld.
// Alle hoeken zijn "logisch" (90 = neutraal); richting en kalibratie worden
// pas bij het schrijven naar de servo toegepast.
#pragma once
#include <ESP32Servo.h>
#include <Preferences.h>
#include "kinematics.h"

struct Frame {
  float a[NUM_SERVOS];   // NAN = deze servo niet veranderen
  uint16_t ms;
};

class Motion {
 public:
  static constexpr uint8_t QUEUE = 48;

  void begin() {
    for (uint8_t t = 0; t < 4; t++) ESP32PWM::allocateTimer(t);
    prefs.begin("calib", false);
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
      offset[i] = prefs.getChar(key('o', i), 0);
      dir[i] = prefs.getChar(key('d', i), SERVO_DIR_DEFAULT[i]);
      cur[i] = from[i] = to[i] = 90.0f;
    }
  }

  void attach() {
    if (attached) return;
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
      servo[i].setPeriodHertz(50);
      servo[i].attach(SERVO_PINS[i], 500, 2400);
      write(i, cur[i]);
    }
    attached = true;
  }

  // Servo's slap: geen houdkracht meer, spaart de accu.
  void relax() {
    clear();
    for (uint8_t i = 0; i < NUM_SERVOS; i++) servo[i].detach();
    attached = false;
  }

  bool isAttached() const { return attached; }

  bool push(const Frame &f) {
    if (count >= QUEUE) return false;
    q[(head + count) % QUEUE] = f;
    q[(head + count) % QUEUE].ms = max<uint16_t>(f.ms, MIN_FRAME_MS);
    count++;
    return true;
  }

  // Pose als één frame in de wachtrij.
  bool pushPose(const Pose &p, uint16_t ms) {
    Frame f;
    memcpy(f.a, p.a, sizeof(f.a));
    f.ms = ms;
    return push(f);
  }

  void clear() {
    count = 0;
    moving = false;
    for (uint8_t i = 0; i < NUM_SERVOS; i++) from[i] = to[i] = cur[i];
  }

  uint8_t queued() const { return count; }
  uint8_t freeSlots() const { return QUEUE - count; }
  bool busy() const { return moving || count > 0; }
  float angle(uint8_t i) const { return cur[i]; }
  // Eindhoek van alles wat al gepland staat (basis voor NAN-waarden).
  float planned(uint8_t i) const {
    for (int k = count - 1; k >= 0; k--) {
      float v = q[(head + k) % QUEUE].a[i];
      if (!isnan(v)) return v;
    }
    return to[i];
  }

  void update() {
    uint32_t now = millis();
    if (!moving && count > 0) {
      Frame &f = q[head];
      head = (head + 1) % QUEUE;
      count--;
      for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        from[i] = cur[i];
        to[i] = isnan(f.a[i]) ? to[i] : constrain(f.a[i], (float)SERVO_MIN_DEG, (float)SERVO_MAX_DEG);
      }
      t0 = now;
      dur = f.ms;
      moving = true;
      attach();
    }
    if (!moving) return;
    float t = min(1.0f, (now - t0) / (float)dur);
    float e = t * t * (3 - 2 * t);  // smoothstep: zacht optrekken en afremmen
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
      float v = from[i] + (to[i] - from[i]) * e;
      if (fabsf(v - cur[i]) > 0.05f || t >= 1.0f) {
        cur[i] = v;
        write(i, v);
      }
    }
    if (t >= 1.0f) moving = false;
  }

  // ---- kalibratie ----
  void setOffset(uint8_t i, int8_t o) { offset[i] = constrain(o, -30, 30); write(i, cur[i]); }
  void setDir(uint8_t i, int8_t d) { dir[i] = d < 0 ? -1 : 1; write(i, cur[i]); }
  int8_t getOffset(uint8_t i) const { return offset[i]; }
  int8_t getDir(uint8_t i) const { return dir[i]; }
  void saveCalib() {
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
      prefs.putChar(key('o', i), offset[i]);
      prefs.putChar(key('d', i), dir[i]);
    }
  }
  void resetCalib() {
    prefs.clear();
    for (uint8_t i = 0; i < NUM_SERVOS; i++) { offset[i] = 0; dir[i] = SERVO_DIR_DEFAULT[i]; }
  }

 private:
  void write(uint8_t i, float logical) {
    if (!servo[i].attached()) return;
    float phys = 90.0f + dir[i] * (logical - 90.0f) + offset[i];
    phys = constrain(phys, 0.0f, 180.0f);
    servo[i].writeMicroseconds(500 + (int)(phys * (1900.0f / 180.0f)));
  }
  const char *key(char c, uint8_t i) {
    keybuf[0] = c; keybuf[1] = 'a' + i; keybuf[2] = 0;
    return keybuf;
  }

  Servo servo[NUM_SERVOS];
  Preferences prefs;
  char keybuf[3];
  int8_t offset[NUM_SERVOS];
  int8_t dir[NUM_SERVOS];
  float cur[NUM_SERVOS], from[NUM_SERVOS], to[NUM_SERVOS];
  Frame q[QUEUE];
  uint8_t head = 0, count = 0;
  bool moving = false, attached = false;
  uint32_t t0 = 0;
  uint16_t dur = 1;
};
