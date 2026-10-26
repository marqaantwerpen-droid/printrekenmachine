// Inverse kinematica voor 3-DOF poten en een eenvoudige draf-gang (trot).
#pragma once
#include <math.h>
#include "config.h"

struct Pose { float a[NUM_SERVOS]; };   // servohoeken in graden (0..180)

namespace kin {

inline float deg(float r) { return r * 180.0f / (float)M_PI; }
inline float rad(float d) { return d * (float)M_PI / 180.0f; }

// Voetpositie in het lokale heup-frame: x = radiaal naar buiten,
// y = tangentieel (CCW), z = omhoog (grond is dus negatief).
// Schrijft drie servohoeken (heup, dij, scheen) naar out[]. Geeft false als
// het punt buiten bereik lag (dan is er geklemd).
inline bool legIK(uint8_t leg, float x, float y, float z, float *out) {
  bool ok = true;
  float coxa = atan2f(y, x);
  float d = sqrtf(x * x + y * y) - COXA_LEN;
  float D = sqrtf(d * d + z * z);
  const float Dmin = fabsf(FEMUR_LEN - TIBIA_LEN) + 2.0f;
  const float Dmax = FEMUR_LEN + TIBIA_LEN - 2.0f;
  if (D < Dmin) { D = Dmin; ok = false; }
  if (D > Dmax) { D = Dmax; ok = false; }
  float femur = atan2f(z, d) +
      acosf((FEMUR_LEN * FEMUR_LEN + D * D - TIBIA_LEN * TIBIA_LEN) / (2 * FEMUR_LEN * D));
  float knee = acosf((FEMUR_LEN * FEMUR_LEN + TIBIA_LEN * TIBIA_LEN - D * D) /
                     (2 * FEMUR_LEN * TIBIA_LEN));
  // Neutraal (90°): heup recht naar buiten, dij horizontaal, scheen haaks op dij.
  // Dit zijn "logische" hoeken; richting en kalibratie past de servodriver toe.
  out[0] = 90.0f + deg(coxa);
  out[1] = 90.0f + deg(femur);
  out[2] = 90.0f + (deg(knee) - 90.0f);
  return ok;
}

// Voetpositie in het lichaamsframe (x vooruit, y links) -> lokaal heup-frame.
inline void bodyToLeg(uint8_t leg, float bx, float by, float &lx, float &ly) {
  float m = rad(LEG_MOUNT_DEG[leg]);
  float hx = BODY_HALF * cosf(m), hy = BODY_HALF * sinf(m);
  float dx = bx - hx, dy = by - hy;
  lx = dx * cosf(m) + dy * sinf(m);
  ly = -dx * sinf(m) + dy * cosf(m);
}

// Rustpositie van een voet in het lichaamsframe.
inline void homeFoot(uint8_t leg, float reach, float &bx, float &by) {
  float m = rad(LEG_MOUNT_DEG[leg]);
  bx = (BODY_HALF + reach) * cosf(m);
  by = (BODY_HALF + reach) * sinf(m);
}

// Bouwt een pose waarin elke voet verplaatst is met (dx, dy, dz) en een
// rotatie yaw (graden) rond het lichaamscentrum. Hoofdservo's blijven gelijk.
inline void footPose(Pose &p, const float dx[NUM_LEGS], const float dy[NUM_LEGS],
                     const float dz[NUM_LEGS], const float yaw[NUM_LEGS],
                     float height, float reach) {
  for (uint8_t l = 0; l < NUM_LEGS; l++) {
    float bx, by;
    homeFoot(l, reach, bx, by);
    float c = cosf(rad(yaw[l])), s = sinf(rad(yaw[l]));
    float rx = bx * c - by * s + dx[l];
    float ry = bx * s + by * c + dy[l];
    float lx, ly;
    bodyToLeg(l, rx, ry, lx, ly);
    legIK(l, lx, ly, -height + dz[l], &p.a[l * 3]);
  }
}

}  // namespace kin
