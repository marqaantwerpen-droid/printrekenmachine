// Gezicht op de 0.91" OLED en de WS2812B-leds.
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

class Face {
 public:
  Face() : oled(128, 32, &Wire, -1) {}

  bool begin() {
    ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (ok) { oled.clearDisplay(); oled.display(); }
    return ok;
  }

  // Expressies: neutral happy sad angry surprised sleep love wink
  bool setExpr(const String &e) {
    static const char *names[] = {"neutral", "happy", "sad", "angry",
                                  "surprised", "sleep", "love", "wink"};
    for (uint8_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      if (e == names[i]) { expr = e; textUntil = 0; dirty = true; return true; }
    return false;
  }
  const String &getExpr() const { return expr; }

  void showText(const String &t, uint32_t ms) {
    text = t.substring(0, 42);
    textUntil = millis() + max<uint32_t>(ms, 500);
    dirty = true;
  }

  // Ogen kijken mee met het hoofd (-1..1).
  void look(float x, float y) { lookX = x; lookY = y; dirty = true; }

  void update() {
    if (!ok) return;
    uint32_t now = millis();
    if (now >= nextBlink) {
      blinkUntil = now + 140;
      nextBlink = now + 2500 + random(4000);
      dirty = true;
    }
    bool blinking = now < blinkUntil;
    if (blinking != wasBlinking) { wasBlinking = blinking; dirty = true; }
    if (textUntil && now > textUntil) { textUntil = 0; dirty = true; }
    if (!dirty || now - lastDraw < 40) return;
    lastDraw = now;
    dirty = false;

    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    if (textUntil) {
      oled.setTextSize(text.length() <= 10 ? 2 : 1);
      oled.setCursor(0, text.length() <= 10 ? 9 : 0);
      oled.print(text);
    } else {
      drawEyes(blinking);
    }
    oled.display();
  }

 private:
  void drawEyes(bool blink) {
    int ox = (int)(lookX * 10), oy = (int)(lookY * 4);
    const int cx[2] = {36 + ox, 92 + ox};
    const int cy = 16 + oy;
    for (uint8_t i = 0; i < 2; i++) {
      int x = cx[i], y = cy;
      bool closed = blink || expr == "sleep" || (expr == "wink" && i == 1);
      if (closed) {
        oled.fillRect(x - 12, y, 24, 3, SSD1306_WHITE);
      } else if (expr == "happy") {
        oled.fillCircle(x, y + 4, 12, SSD1306_WHITE);
        oled.fillCircle(x, y + 10, 12, SSD1306_BLACK);
      } else if (expr == "sad") {
        oled.fillRoundRect(x - 11, y - 9, 22, 18, 5, SSD1306_WHITE);
        oled.fillTriangle(x - 13, y - 11, x + 13, y - 11, i ? x + 13 : x - 13, y - 1, SSD1306_BLACK);
      } else if (expr == "angry") {
        oled.fillRoundRect(x - 11, y - 9, 22, 18, 5, SSD1306_WHITE);
        oled.fillTriangle(x - 13, y - 11, x + 13, y - 11, i ? x - 13 : x + 13, y - 1, SSD1306_BLACK);
      } else if (expr == "surprised") {
        oled.drawCircle(x, y, 12, SSD1306_WHITE);
        oled.fillCircle(x, y, 5, SSD1306_WHITE);
      } else if (expr == "love") {
        oled.fillCircle(x - 5, y - 3, 6, SSD1306_WHITE);
        oled.fillCircle(x + 5, y - 3, 6, SSD1306_WHITE);
        oled.fillTriangle(x - 11, y - 1, x + 11, y - 1, x, y + 11, SSD1306_WHITE);
      } else {
        oled.fillRoundRect(x - 11, y - 10, 22, 20, 6, SSD1306_WHITE);
      }
    }
    if (expr == "sleep") {
      oled.setTextSize(1);
      oled.setCursor(112, 0);
      oled.print("z");
    }
  }

  Adafruit_SSD1306 oled;
  bool ok = false, dirty = true, wasBlinking = false;
  String expr = "neutral", text;
  uint32_t textUntil = 0, nextBlink = 3000, blinkUntil = 0, lastDraw = 0;
  float lookX = 0, lookY = 0;
};

class Leds {
 public:
  Leds() : strip(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800) {}

  void begin() { strip.begin(); strip.setBrightness(brightness); strip.show(); }

  // Modi: off solid breathe blink rainbow scan
  bool set(const String &m, uint32_t c, int b) {
    if (m != "off" && m != "solid" && m != "breathe" && m != "blink" &&
        m != "rainbow" && m != "scan") return false;
    mode = m;
    color = c;
    if (b >= 0) brightness = constrain(b, 0, 255);
    return true;
  }

  // Kort flitsen zonder de ingestelde modus te verliezen.
  void flash(uint32_t c, uint16_t ms) { flashColor = c; flashUntil = millis() + ms; }

  const String &getMode() const { return mode; }
  uint32_t getColor() const { return color; }
  uint8_t getBrightness() const { return brightness; }

  void update() {
    uint32_t now = millis();
    if (now - last < 20) return;
    last = now;
    strip.setBrightness(brightness);
    if (now < flashUntil) { fill(flashColor); strip.show(); return; }
    if (mode == "off") fill(0);
    else if (mode == "solid") fill(color);
    else if (mode == "breathe") fill(scale(color, (sinf(now / 600.0f) + 1) * 0.5f));
    else if (mode == "blink") fill((now / 400) % 2 ? color : 0);
    else if (mode == "rainbow")
      for (uint8_t i = 0; i < NUM_LEDS; i++)
        strip.setPixelColor(i, strip.ColorHSV((now * 40 + i * 65536 / NUM_LEDS) & 0xFFFF));
    else if (mode == "scan") {
      uint8_t p = (now / 90) % (NUM_LEDS * 2 - 2);
      if (p >= NUM_LEDS) p = NUM_LEDS * 2 - 2 - p;
      for (uint8_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, i == p ? color : 0);
    }
    strip.show();
  }

 private:
  void fill(uint32_t c) { for (uint8_t i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, c); }
  static uint32_t scale(uint32_t c, float f) {
    return ((uint32_t)(((c >> 16) & 255) * f) << 16) | ((uint32_t)(((c >> 8) & 255) * f) << 8) |
           (uint32_t)((c & 255) * f);
  }
  Adafruit_NeoPixel strip;
  String mode = "breathe";
  uint32_t color = 0x0040FF, flashColor = 0, flashUntil = 0, last = 0;
  uint8_t brightness = 40;
};
