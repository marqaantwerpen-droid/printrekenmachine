// SPIN — een 3D-geprinte vierpotige WiFi-robot (ESP32 + 14x MG90S).
// Bediening via JSON-API op http://spin.local/api/... of de webpagina op /.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <deque>
#include <vector>
#include "config.h"
#include "secrets.h"
#include "kinematics.h"
#include "motion.h"
#include "face.h"
#include "web_ui.h"

#define FW_VERSION "1.0.0"

WebServer server(80);
Motion motion;
Face face;
Leds leds;

// ------------------------------------------------------------ gebeurtenissen
struct Event { uint32_t t; String msg; };
std::deque<Event> events;
void logEvent(const String &m) {
  events.push_back({millis() / 1000, m});
  if (events.size() > 12) events.pop_front();
  Serial.println("[event] " + m);
}

// ------------------------------------------------------------ toestand
String action = "idle";
float bodyHeight = STAND_HEIGHT, bodyReach = STAND_REACH;

struct WalkState {
  int remaining = 0;          // halve stappen te gaan
  bool settle = false;        // nog een afsluitende stap om netjes te staan
  uint8_t pair = 0;           // welk diagonaal paar zwaait er nu
  float s[NUM_LEGS] = {0, 0, 0, 0};
  float ux = 0, uy = 0, turn = 0;   // richting en draaiing per eenheid
  float stride = 40, lift = 25;
  uint16_t frameMs = 90;
} walk;

struct PlayState {
  std::vector<Frame> frames;
  size_t idx = 0;
  int repeat = 0;
  String name;
} play;

std::deque<String> cmdQueue;       // opeenvolgende commando's (batch)
constexpr size_t CMD_QUEUE_MAX = 32;

struct Hall { int raw = 0; bool magnet = false; uint32_t count = 0; } hall;

// ------------------------------------------------------------ helpers
uint32_t parseColor(const String &s) {
  if (s.startsWith("#")) return strtoul(s.c_str() + 1, nullptr, 16);
  if (s == "red") return 0xFF0000;
  if (s == "green") return 0x00FF00;
  if (s == "blue") return 0x0040FF;
  if (s == "yellow") return 0xFFB000;
  if (s == "purple") return 0x9000FF;
  if (s == "white") return 0xFFFFFF;
  if (s == "orange") return 0xFF5000;
  if (s == "pink") return 0xFF2080;
  return 0x0040FF;
}

bool validName(const String &n) {
  if (n.length() == 0 || n.length() > 24) return false;
  for (char c : n)
    if (!(isalnum(c) || c == '_' || c == '-')) return false;
  return true;
}

void currentPose(Pose &p) {
  for (uint8_t i = 0; i < NUM_SERVOS; i++) p.a[i] = motion.planned(i);
}

void standPose(Pose &p, float height, float reach, const float dz[NUM_LEGS] = nullptr) {
  const float zero[NUM_LEGS] = {0, 0, 0, 0};
  currentPose(p);
  kin::footPose(p, zero, zero, dz ? dz : zero, zero, height, reach);
}

void stopAll() {
  walk.remaining = 0;
  walk.settle = false;
  play.frames.clear();
  play.repeat = 0;
  cmdQueue.clear();
  motion.clear();
  action = "idle";
}

// ------------------------------------------------------------ lopen
// Eén halve stap: het zwaaiende paar tilt op en gaat naar T, het
// steunende paar schuift over de grond naar -T (dat duwt het lichaam).
void pushHalfStep(float T) {
  const uint8_t pairs[2][2] = {{0, 3}, {1, 2}};   // FL+RR, FR+RL
  const uint8_t N = 4;
  float start[NUM_LEGS];
  memcpy(start, walk.s, sizeof(start));
  for (uint8_t k = 1; k <= N; k++) {
    float t = k / (float)N;
    float dx[NUM_LEGS], dy[NUM_LEGS], dz[NUM_LEGS], yaw[NUM_LEGS];
    for (uint8_t l = 0; l < NUM_LEGS; l++) {
      bool swing = (l == pairs[walk.pair][0] || l == pairs[walk.pair][1]);
      float target = swing ? T : -T;
      float s = start[l] + (target - start[l]) * t;
      walk.s[l] = s;
      dx[l] = s * walk.stride * walk.ux;
      dy[l] = s * walk.stride * walk.uy;
      yaw[l] = s * walk.turn;
      dz[l] = swing ? walk.lift * sinf((float)M_PI * t) : 0;
    }
    Pose p;
    currentPose(p);
    kin::footPose(p, dx, dy, dz, yaw, bodyHeight, bodyReach);
    Frame f;
    memcpy(f.a, p.a, sizeof(f.a));
    f.a[12] = f.a[13] = NAN;   // hoofd niet aanraken
    f.ms = walk.frameMs;
    motion.push(f);
  }
  walk.pair ^= 1;
}

void serviceWalk() {
  while (motion.freeSlots() >= 4 && (walk.remaining > 0 || walk.settle)) {
    if (walk.remaining > 0) {
      pushHalfStep(0.5f);
      if (--walk.remaining == 0) walk.settle = true;
    } else {
      pushHalfStep(0.0f);
      walk.settle = false;
    }
  }
}

String startWalk(const String &dir, int steps, float speed, float stride) {
  walk = WalkState();
  if (dir == "forward") walk.ux = 1;
  else if (dir == "backward") walk.ux = -1;
  else if (dir == "left") walk.uy = 1;
  else if (dir == "right") walk.uy = -1;
  else if (dir == "turn_left") walk.turn = 22;
  else if (dir == "turn_right") walk.turn = -22;
  else return "onbekende richting (forward|backward|left|right|turn_left|turn_right)";
  speed = constrain(speed, 0.3f, 2.0f);
  walk.stride = constrain(stride, 10.0f, 60.0f);
  walk.frameMs = (uint16_t)(90 / speed);
  walk.remaining = constrain(steps, 1, (int)MAX_WALK_STEPS) * 2;   // 1 stap = 2 halve stappen
  bodyHeight = max(bodyHeight, MIN_WALK_HEIGHT);   // lager kan de poot niet ver genoeg optillen
  // Eerst rustig rechtop gaan staan.
  Pose p;
  standPose(p, bodyHeight, bodyReach);
  motion.pushPose(p, 300);
  action = "walk " + dir;
  return "";
}

// ------------------------------------------------------------ ingebouwde moves
const char *BUILTIN_MOVES[] = {"wave", "bow", "dance", "shake", "stretch", "look_around", "hello"};

void pushHead(float pan, float tilt, uint16_t ms) {
  Frame f;
  for (uint8_t i = 0; i < NUM_SERVOS; i++) f.a[i] = NAN;
  f.a[12] = constrain(pan, 40.0f, 140.0f);
  f.a[13] = constrain(tilt, 50.0f, 130.0f);
  f.ms = ms;
  motion.push(f);
}

bool runBuiltin(const String &n) {
  const float z[NUM_LEGS] = {0, 0, 0, 0};
  Pose p;
  if (n == "wave" || n == "hello") {
    // Gewicht naar achteren, linker voorpoot zwaait.
    const float dz[NUM_LEGS] = {0, -8, 10, -5};
    standPose(p, bodyHeight, bodyReach, dz);
    motion.pushPose(p, 400);
    for (int i = 0; i < 3; i++) {
      Pose q = p;
      q.a[0] = 110; q.a[1] = 150; q.a[2] = 60;
      motion.pushPose(q, 250);
      q.a[0] = 70;
      motion.pushPose(q, 250);
    }
    if (n == "hello") face.showText("Hallo!", 2000);
  } else if (n == "bow") {
    const float dz[NUM_LEGS] = {28, 28, -10, -10};
    standPose(p, bodyHeight, bodyReach, dz);
    motion.pushPose(p, 600);
    Frame hold; for (auto &v : hold.a) v = NAN; hold.ms = 600; motion.push(hold);
  } else if (n == "dance") {
    for (int i = 0; i < 4; i++) {
      const float up[NUM_LEGS] = {-12, 12, -12, 12};
      const float dn[NUM_LEGS] = {12, -12, 12, -12};
      standPose(p, bodyHeight, bodyReach, up); motion.pushPose(p, 220);
      standPose(p, bodyHeight, bodyReach, dn); motion.pushPose(p, 220);
    }
  } else if (n == "shake") {
    for (int i = 0; i < 4; i++) {
      const float yl[NUM_LEGS] = {12, 12, 12, 12};
      const float yr[NUM_LEGS] = {-12, -12, -12, -12};
      currentPose(p); kin::footPose(p, z, z, z, yl, bodyHeight, bodyReach); motion.pushPose(p, 160);
      currentPose(p); kin::footPose(p, z, z, z, yr, bodyHeight, bodyReach); motion.pushPose(p, 160);
    }
  } else if (n == "stretch") {
    standPose(p, bodyHeight + 20, bodyReach - 15); motion.pushPose(p, 800);
    standPose(p, REST_HEIGHT, bodyReach + 15); motion.pushPose(p, 900);
  } else if (n == "look_around") {
    pushHead(50, 90, 600); pushHead(130, 90, 1000); pushHead(90, 70, 600); pushHead(90, 90, 400);
    return true;
  } else {
    return false;
  }
  standPose(p, bodyHeight, bodyReach);
  motion.pushPose(p, 400);
  return true;
}

// ------------------------------------------------------------ eigen moves (LittleFS)
String movePath(const String &n) { return "/moves/" + n + ".json"; }

bool loadMove(const String &n, std::vector<Frame> &out, String &err) {
  File f = LittleFS.open(movePath(n), "r");
  if (!f) { err = "move niet gevonden"; return false; }
  JsonDocument doc;
  if (deserializeJson(doc, f)) { err = "move-bestand beschadigd"; return false; }
  out.clear();
  for (JsonObject fr : doc["frames"].as<JsonArray>()) {
    Frame x;
    JsonArray a = fr["a"];
    for (uint8_t i = 0; i < NUM_SERVOS; i++)
      x.a[i] = (i < a.size() && !a[i].isNull()) ? a[i].as<float>() : NAN;
    x.ms = fr["ms"] | 300;
    out.push_back(x);
  }
  return !out.empty();
}

void listMoves(JsonArray arr) {
  for (auto n : BUILTIN_MOVES) { JsonObject o = arr.add<JsonObject>(); o["name"] = n; o["builtin"] = true; }
  File dir = LittleFS.open("/moves");
  if (!dir) return;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String n = f.name();
    if (!n.endsWith(".json")) continue;
    n = n.substring(n.lastIndexOf('/') + 1, n.length() - 5);
    JsonObject o = arr.add<JsonObject>();
    o["name"] = n;
    o["builtin"] = false;
  }
}

void servicePlay() {
  while (!play.frames.empty() && motion.freeSlots() > 0) {
    motion.push(play.frames[play.idx]);
    if (++play.idx >= play.frames.size()) {
      play.idx = 0;
      if (--play.repeat <= 0) { play.frames.clear(); break; }
    }
  }
}

// ------------------------------------------------------------ commando's
bool isMotionCmd(const String &c) {
  return c == "pose" || c == "walk" || c == "servo" || c == "head" || c == "play" ||
         c == "wait" || c == "wake" || c == "height";
}

// Voert één commando uit. Geeft een foutmelding terug, of "" bij succes.
String execute(JsonObject c) {
  String cmd = c["cmd"] | "";

  if (cmd == "stop") { stopAll(); logEvent("stop"); return ""; }

  if (cmd == "relax") { stopAll(); motion.relax(); action = "relaxed"; face.setExpr("sleep"); return ""; }

  if (cmd == "wake") {
    face.setExpr("neutral");
    Pose p; standPose(p, bodyHeight, bodyReach);
    motion.pushPose(p, 700);
    action = "stand";
    return "";
  }

  if (cmd == "pose") {
    String n = c["name"] | "stand";
    uint16_t ms = c["ms"] | 600;
    Pose p;
    if (n == "stand") {
      bodyHeight = constrain(c["height"] | bodyHeight, 35.0f, 90.0f);
      bodyReach = constrain(c["reach"] | bodyReach, 70.0f, 120.0f);
      standPose(p, bodyHeight, bodyReach);
    } else if (n == "sit") {
      const float dz[NUM_LEGS] = {-5, -5, 30, 30};
      standPose(p, bodyHeight, bodyReach, dz);
    } else if (n == "rest") {
      standPose(p, REST_HEIGHT, bodyReach + 10);
    } else if (n == "neutral") {
      for (auto &v : p.a) v = 90;
    } else {
      return "onbekende pose (stand|sit|rest|neutral)";
    }
    motion.pushPose(p, ms);
    action = "pose " + n;
    return "";
  }

  if (cmd == "height") {   // lichaam hoger/lager zonder te lopen
    bodyHeight = constrain(c["height"] | bodyHeight, 35.0f, 90.0f);
    Pose p; standPose(p, bodyHeight, bodyReach);
    motion.pushPose(p, c["ms"] | 400);
    return "";
  }

  if (cmd == "walk") {
    return startWalk(c["dir"] | "forward", c["steps"] | 2, c["speed"] | 1.0f, c["stride"] | 40.0f);
  }

  if (cmd == "servo") {
    Frame f;
    for (auto &v : f.a) v = NAN;
    f.ms = c["ms"] | 300;
    if (c["angles"].is<JsonArray>()) {
      JsonArray a = c["angles"];
      for (uint8_t i = 0; i < NUM_SERVOS && i < a.size(); i++)
        if (!a[i].isNull()) f.a[i] = a[i].as<float>();
    } else {
      int id = c["id"] | -1;
      if (id < 0 || id >= NUM_SERVOS) return "id moet 0..13 zijn";
      f.a[id] = c["angle"] | 90.0f;
    }
    motion.push(f);
    action = "servo";
    return "";
  }

  if (cmd == "head") {
    float pan = c["pan"] | motion.planned(12), tilt = c["tilt"] | motion.planned(13);
    pushHead(pan, tilt, c["ms"] | 300);
    face.look((pan - 90) / 50.0f, (tilt - 90) / 40.0f);
    return "";
  }

  if (cmd == "wait") {
    Frame f; for (auto &v : f.a) v = NAN; f.ms = constrain((int)(c["ms"] | 500), 40, 10000);
    motion.push(f);
    return "";
  }

  if (cmd == "play") {
    String n = c["name"] | "";
    int rep = constrain((int)(c["repeat"] | 1), 1, 20);
    if (runBuiltin(n)) {
      // Herhalingen pas inplannen als deze ronde klaar is (past niet in één wachtrij).
      if (rep > 1) cmdQueue.push_front("{\"cmd\":\"play\",\"name\":\"" + n + "\",\"repeat\":" + String(rep - 1) + "}");
    } else {
      String err;
      if (!loadMove(n, play.frames, err)) return err;
      play.idx = 0;
      play.repeat = rep;
      play.name = n;
    }
    action = "play " + n;
    return "";
  }

  if (cmd == "face") {
    if (c["text"].is<const char *>()) { face.showText(c["text"].as<String>(), c["ms"] | 3000); return ""; }
    if (!face.setExpr(c["expr"] | "")) return "onbekende expressie (neutral|happy|sad|angry|surprised|sleep|love|wink)";
    return "";
  }

  if (cmd == "led") {
    if (!leds.set(c["mode"] | "solid", parseColor(c["color"] | "#0040FF"), c["brightness"] | -1))
      return "onbekende led-modus (off|solid|breathe|blink|rainbow|scan)";
    return "";
  }

  if (cmd == "save_move") {
    String n = c["name"] | "";
    if (!validName(n)) return "naam: 1-24 tekens, a-z 0-9 _ -";
    for (auto b : BUILTIN_MOVES) if (n == b) return "dat is een ingebouwde move";
    JsonArray frames = c["frames"];
    if (frames.isNull() || frames.size() == 0 || frames.size() > 64) return "frames: 1..64 stuks";
    JsonDocument out;
    JsonArray of = out["frames"].to<JsonArray>();
    for (JsonObject fr : frames) {
      JsonObject o = of.add<JsonObject>();
      o["ms"] = constrain((int)(fr["ms"] | 300), (int)MIN_FRAME_MS, 5000);
      JsonArray a = o["a"].to<JsonArray>();
      JsonArray in = fr["a"];
      for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        if (i < in.size() && !in[i].isNull()) a.add(constrain(in[i].as<float>(), (float)SERVO_MIN_DEG, (float)SERVO_MAX_DEG));
        else a.add<JsonVariant>();   // null = servo niet veranderen
      }
    }
    File f = LittleFS.open(movePath(n), "w");
    if (!f) return "kan niet opslaan";
    serializeJson(out, f);
    f.close();
    logEvent("move '" + n + "' geleerd (" + String(frames.size()) + " frames)");
    face.setExpr("happy");
    return "";
  }

  if (cmd == "delete_move") {
    String n = c["name"] | "";
    if (!validName(n) || !LittleFS.remove(movePath(n))) return "move niet gevonden";
    logEvent("move '" + n + "' verwijderd");
    return "";
  }

  if (cmd == "calibrate") {
    int id = c["id"] | -1;
    if (id < 0 || id >= NUM_SERVOS) return "id moet 0..13 zijn";
    if (!c["offset"].isNull()) motion.setOffset(id, c["offset"].as<int>());
    if (!c["dir"].isNull()) motion.setDir(id, c["dir"].as<int>());
    if (c["save"] | false) motion.saveCalib();
    return "";
  }
  if (cmd == "calibrate_save") { motion.saveCalib(); logEvent("kalibratie opgeslagen"); return ""; }
  if (cmd == "calibrate_reset") { motion.resetCalib(); logEvent("kalibratie gewist"); return ""; }

  return "onbekend commando: '" + cmd + "' (zie GET /api/help)";
}

// Commando's van buitenaf: bewegingen wachten netjes op hun beurt; overige
// commando's (led, face, ...) werken direct, behalve binnen een batch.
String submit(JsonVariant body, int &queued) {
  queued = 0;
  if (body.is<JsonArray>()) {
    if (cmdQueue.size() + body.size() > CMD_QUEUE_MAX) return "wachtrij vol";
    for (JsonVariant v : body.as<JsonArray>()) {
      String s; serializeJson(v, s); cmdQueue.push_back(s); queued++;
    }
    return "";
  }
  JsonObject c = body.as<JsonObject>();
  String cmd = c["cmd"] | "";
  if (cmd == "stop" || !isMotionCmd(cmd)) return execute(c);
  if (cmdQueue.size() >= CMD_QUEUE_MAX) return "wachtrij vol";
  String s; serializeJson(c, s); cmdQueue.push_back(s); queued = 1;
  return "";
}

bool generatorsBusy() { return walk.remaining > 0 || walk.settle || !play.frames.empty(); }

void serviceCommands() {
  while (!cmdQueue.empty() && !motion.busy() && !generatorsBusy()) {
    JsonDocument doc;
    deserializeJson(doc, cmdQueue.front());
    cmdQueue.pop_front();
    String err = execute(doc.as<JsonObject>());
    if (err.length()) logEvent("fout: " + err);
  }
  if (!motion.busy() && !generatorsBusy() && cmdQueue.empty() &&
      action != "idle" && action != "relaxed" && !action.startsWith("pose"))
    action = "idle";
}

// ------------------------------------------------------------ sensoren
void serviceSensors() {
  static uint32_t last = 0;
  if (millis() - last < 50) return;
  last = millis();
  hall.raw = analogRead(PIN_HALL);
  bool m = HALL_ACTIVE_LOW ? hall.raw < 1800 : hall.raw > 2300;
  if (m != hall.magnet) {
    hall.magnet = m;
    if (m) {
      hall.count++;
      logEvent("magneet gedetecteerd (hall)");
      leds.flash(0x00FF00, 400);
      face.setExpr("happy");
    } else {
      logEvent("magneet weg");
    }
  }
}

float readVbat() {
  uint32_t mv = analogReadMilliVolts(PIN_VBAT);
  float v = mv * VBAT_DIVIDER / 1000.0f;
  return v > 3.0f ? v : NAN;   // niets aangesloten
}

// ------------------------------------------------------------ HTTP
bool authorized() {
  if (strlen(API_KEY) == 0) return true;
  if (server.header("X-API-Key") == API_KEY || server.arg("key") == API_KEY) return true;
  server.send(401, "application/json", "{\"ok\":false,\"error\":\"API-sleutel ontbreekt\"}");
  return false;
}

void sendJson(int code, JsonDocument &doc) {
  String s; serializeJson(doc, s);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", s);
}

void handleStatus() {
  if (!authorized()) return;
  JsonDocument d;
  d["name"] = ROBOT_NAME;
  d["version"] = FW_VERSION;
  d["ip"] = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  d["wifi"] = WiFi.getMode() == WIFI_AP ? "ap" : "sta";
  d["rssi"] = WiFi.RSSI();
  d["uptime_s"] = millis() / 1000;
  d["action"] = action;
  d["busy"] = motion.busy() || generatorsBusy() || !cmdQueue.empty();
  d["motion_queue"] = motion.queued();
  d["cmd_queue"] = cmdQueue.size();
  d["servos_on"] = motion.isAttached();
  d["body"]["height"] = bodyHeight;
  d["body"]["reach"] = bodyReach;
  JsonArray a = d["angles"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_SERVOS; i++) a.add(roundf(motion.angle(i) * 10) / 10);
  d["face"] = face.getExpr();
  d["led"]["mode"] = leds.getMode();
  char col[8]; snprintf(col, sizeof(col), "#%06X", (unsigned)leds.getColor());
  d["led"]["color"] = col;
  d["led"]["brightness"] = leds.getBrightness();
  d["hall"]["raw"] = hall.raw;
  d["hall"]["magnet"] = hall.magnet;
  d["hall"]["count"] = hall.count;
  float vb = readVbat();
  if (isnan(vb)) d["vbat"] = nullptr; else d["vbat"] = roundf(vb * 100) / 100;
  d["free_heap"] = ESP.getFreeHeap();
  JsonArray ev = d["events"].to<JsonArray>();
  for (auto &e : events) { JsonObject o = ev.add<JsonObject>(); o["t"] = e.t; o["msg"] = e.msg; }
  listMoves(d["moves"].to<JsonArray>());
  sendJson(200, d);
}

void handleCmd() {
  if (!authorized()) return;
  JsonDocument body, res;
  if (deserializeJson(body, server.arg("plain"))) {
    res["ok"] = false; res["error"] = "ongeldige JSON";
    sendJson(400, res);
    return;
  }
  int queued = 0;
  String err = submit(body.as<JsonVariant>(), queued);
  res["ok"] = err.length() == 0;
  if (err.length()) res["error"] = err;
  res["queued"] = queued;
  res["action"] = action;
  sendJson(err.length() ? 400 : 200, res);
}

void handleGetMove() {
  if (!authorized()) return;
  File f = LittleFS.open(movePath(server.arg("name")), "r");
  if (!validName(server.arg("name")) || !f) { server.send(404, "application/json", "{\"ok\":false}"); return; }
  server.streamFile(f, "application/json");
}

void handleCalib() {
  if (!authorized()) return;
  JsonDocument d;
  JsonArray a = d["servos"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    JsonObject o = a.add<JsonObject>();
    o["id"] = i; o["pin"] = SERVO_PINS[i]; o["offset"] = motion.getOffset(i); o["dir"] = motion.getDir(i);
  }
  sendJson(200, d);
}

void setupHttp() {
  const char *hdrs[] = {"X-API-Key"};
  server.collectHeaders(hdrs, 1);
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/help", HTTP_GET, [] { server.send_P(200, "text/plain; charset=utf-8", API_HELP); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/move", HTTP_GET, handleGetMove);
  server.on("/api/calibration", HTTP_GET, handleCalib);
  server.on("/api/cmd", HTTP_OPTIONS, [] {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
    server.send(204);
  });
  server.onNotFound([] { server.send(404, "application/json", "{\"ok\":false,\"error\":\"zie /api/help\"}"); });
  server.begin();
}

// ------------------------------------------------------------ WiFi
void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(ROBOT_NAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  face.showText("WiFi...", 15000);
  face.update();
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    logEvent("WiFi verbonden: " + WiFi.localIP().toString());
    face.showText(WiFi.localIP().toString(), 6000);
  } else {
    // Geen netwerk: eigen hotspot, bereikbaar op 192.168.4.1
    WiFi.mode(WIFI_AP);
    WiFi.softAP("SPIN-robot", "spinrobot");
    logEvent("hotspot SPIN-robot actief (192.168.4.1)");
    face.showText("AP 192.168.4.1", 8000);
  }
  MDNS.begin(ROBOT_NAME);
  MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(ROBOT_NAME);
  if (strlen(API_KEY)) ArduinoOTA.setPassword(API_KEY);
  ArduinoOTA.onStart([] { stopAll(); motion.relax(); face.showText("Update...", 60000); });
  ArduinoOTA.begin();
}

// ------------------------------------------------------------ main
void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  analogReadResolution(12);
  pinMode(PIN_HALL, INPUT);
  if (!face.begin()) Serial.println("OLED niet gevonden (adres 0x3C?)");
  leds.begin();
  if (!LittleFS.begin(true)) Serial.println("LittleFS mislukt");
  LittleFS.mkdir("/moves");
  motion.begin();
  setupWifi();
  setupHttp();
  // Rustig opstaan vanuit liggende houding.
  Pose p;
  standPose(p, REST_HEIGHT, STAND_REACH + 10);
  p.a[12] = p.a[13] = 90;
  motion.pushPose(p, 800);
  standPose(p, bodyHeight, bodyReach);
  motion.pushPose(p, 900);
  action = "pose stand";
  logEvent("SPIN " FW_VERSION " gestart");
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  serviceWalk();
  servicePlay();
  motion.update();
  serviceCommands();
  serviceSensors();
  face.update();
  leds.update();
}
