#!/usr/bin/env python3
"""Bedien de SPIN-robot vanaf de commandoregel (geen extra pakketten nodig).

Voorbeelden:
  python3 spin.py status
  python3 spin.py walk forward 3
  python3 spin.py walk turn_left 2 --speed 1.5
  python3 spin.py pose sit
  python3 spin.py play wave
  python3 spin.py face happy
  python3 spin.py say "Hallo Marqa"
  python3 spin.py led rainbow
  python3 spin.py head 60 100
  python3 spin.py servo 4 120
  python3 spin.py raw '[{"cmd":"walk","dir":"forward","steps":2},{"cmd":"play","name":"bow"}]'
  python3 spin.py learn mijn_dans frames.json
  python3 spin.py stop

Robot-adres: --host, of omgevingsvariabele SPIN_HOST (standaard spin.local).
"""
import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request


def request(host, path, body=None, key=""):
    url = f"http://{host}{path}"
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method="POST" if data else "GET")
    req.add_header("Content-Type", "application/json")
    if key:
        req.add_header("X-API-Key", key)
    try:
        with urllib.request.urlopen(req, timeout=8) as r:
            raw = r.read().decode()
    except urllib.error.HTTPError as e:
        raw = e.read().decode()
    except urllib.error.URLError as e:
        sys.exit(f"Robot niet bereikbaar op {host}: {e.reason}")
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        return raw


def main():
    p = argparse.ArgumentParser(description="SPIN robot bediening")
    p.add_argument("--host", default=os.environ.get("SPIN_HOST", "spin.local"))
    p.add_argument("--key", default=os.environ.get("SPIN_KEY", ""))
    p.add_argument("--speed", type=float, default=1.0)
    p.add_argument("--ms", type=int, default=None)
    p.add_argument("--wait", action="store_true", help="wacht tot de robot klaar is")
    p.add_argument("action")
    p.add_argument("args", nargs="*")
    a = p.parse_args()
    x = a.args

    def cmd(c):
        if a.ms is not None:
            c["ms"] = a.ms
        return request(a.host, "/api/cmd", c, a.key)

    act = a.action
    if act == "status":
        res = request(a.host, "/api/status", key=a.key)
    elif act == "help":
        res = request(a.host, "/api/help", key=a.key)
    elif act == "walk":
        res = cmd({"cmd": "walk", "dir": x[0] if x else "forward",
                   "steps": int(x[1]) if len(x) > 1 else 2, "speed": a.speed})
    elif act in ("stop", "relax", "wake"):
        res = request(a.host, "/api/cmd", {"cmd": act}, a.key)
    elif act == "pose":
        c = {"cmd": "pose", "name": x[0] if x else "stand"}
        if len(x) > 1:
            c["height"] = float(x[1])
        res = cmd(c)
    elif act == "play":
        res = cmd({"cmd": "play", "name": x[0], "repeat": int(x[1]) if len(x) > 1 else 1})
    elif act == "face":
        res = cmd({"cmd": "face", "expr": x[0]})
    elif act == "say":
        res = cmd({"cmd": "face", "text": " ".join(x)})
    elif act == "led":
        c = {"cmd": "led", "mode": x[0]}
        if len(x) > 1:
            c["color"] = x[1]
        if len(x) > 2:
            c["brightness"] = int(x[2])
        res = cmd(c)
    elif act == "head":
        res = cmd({"cmd": "head", "pan": float(x[0]), "tilt": float(x[1]) if len(x) > 1 else 90})
    elif act == "servo":
        res = cmd({"cmd": "servo", "id": int(x[0]), "angle": float(x[1])})
    elif act == "calibrate":
        res = cmd({"cmd": "calibrate", "id": int(x[0]), "offset": int(x[1]), "save": True})
    elif act == "learn":
        with open(x[1]) as f:
            frames = json.load(f)
        res = cmd({"cmd": "save_move", "name": x[0], "frames": frames})
    elif act == "forget":
        res = cmd({"cmd": "delete_move", "name": x[0]})
    elif act == "raw":
        res = request(a.host, "/api/cmd", json.loads(x[0]), a.key)
    else:
        sys.exit(f"Onbekende actie '{act}'. Zie: python3 spin.py --help")

    print(res if isinstance(res, str) else json.dumps(res, indent=2, ensure_ascii=False))

    if a.wait and act not in ("status", "help"):
        while True:
            time.sleep(0.5)
            s = request(a.host, "/api/status", key=a.key)
            if not s.get("busy"):
                print(f"klaar — actie: {s.get('action')}")
                break


if __name__ == "__main__":
    main()
