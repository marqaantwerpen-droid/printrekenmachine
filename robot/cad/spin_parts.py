#!/usr/bin/env python3
"""Genereert alle 3D-printbare onderdelen van SPIN als STL.

    pip install manifold3d trimesh numpy
    python3 spin_parts.py            # schrijft naar ./stl/

Alle maten in mm. Afmetingen van de MG90S en de servohoorn staan bovenaan;
meet je eigen exemplaren na en pas zo nodig aan voor je de rest print.
"""
import math
import os

import numpy as np
import trimesh
from manifold3d import Manifold

# ----------------------------------------------------------- MG90S-maten
SERVO_L = 22.8 + 0.5      # lengte behuizing (+speling)
SERVO_W = 12.2 + 0.5      # breedte
SERVO_SHAFT_OFS = 5.3     # as t.o.v. midden van de behuizing (langs de lengte)
SERVO_HOLE_SPACING = 27.6 # hart-op-hart schroefgaten in de lipjes
SCREW_PILOT = 1.8         # M2 zelftappers

# Enkelarmige servohoorn (meegeleverd bij MG90S)
HORN_HUB_D = 7.6
HORN_ARM_LEN = 17.0
HORN_ARM_W0, HORN_ARM_W1 = 5.8, 4.2
HORN_DEPTH = 2.0

# Geometrie — gelijk aan firmware/src/config.h
BODY_HALF = 55.0
COXA_LEN = 30.0
FEMUR_LEN = 50.0
TIBIA_LEN = 70.0

SEG = 64


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl(r, h, z0=0.0, seg=SEG):
    return Manifold.cylinder(h, r, r, seg).translate([0, 0, z0])


def cyl_y(r, y0, y1, x=0.0, z=0.0):
    """Cilinder langs de y-as."""
    return Manifold.cylinder(y1 - y0, r, r, SEG).rotate([-90, 0, 0]).translate([x, y0, z])


def union(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out + p
    return out


def rounded_plate(w, d, r, h, z0=0.0):
    pts = []
    for cx, cy, a0 in ((w / 2 - r, d / 2 - r, 0), (-w / 2 + r, d / 2 - r, 90),
                       (-w / 2 + r, -d / 2 + r, 180), (w / 2 - r, -d / 2 + r, 270)):
        for i in range(9):
            a = math.radians(a0 + i * 90 / 8)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    from manifold3d import CrossSection
    return Manifold.extrude(CrossSection([pts]), h).translate([0, 0, z0])


def horn_recess():
    """Uitsparing (in -z vanaf z=0) waarin de servohoorn valt, plus
    doorgang voor de centrale schroef en voorboorgaatjes voor de hoornschroefjes."""
    from manifold3d import CrossSection
    arm = CrossSection([[(0, -HORN_ARM_W0 / 2), (HORN_ARM_LEN, -HORN_ARM_W1 / 2),
                         (HORN_ARM_LEN, HORN_ARM_W1 / 2), (0, HORN_ARM_W0 / 2)]])
    r = Manifold.extrude(arm, HORN_DEPTH) + cyl(HORN_HUB_D / 2, HORN_DEPTH)
    r = r + cyl(3.0, 20, -10)                     # schroevendraaier / schroefkop
    for x in (8.0, 11.5, 14.5):
        r = r + cyl(0.7, 20, -10).translate([x, 0, 0])
    return r  # z van 0 tot HORN_DEPTH: zet hem op het oppervlak dat de hoorn raakt


def servo_window(cx, cz, plate_y0, plate_y1):
    """Rechthoekig gat in een plaat in het xz-vlak voor een MG90S waarvan de
    lengte langs x ligt, plus de twee schroefgaatjes."""
    cut = box(cx - SERVO_L / 2, cx + SERVO_L / 2, plate_y0 - 1, plate_y1 + 1,
              cz - SERVO_W / 2, cz + SERVO_W / 2)
    for dx in (-SERVO_HOLE_SPACING / 2, SERVO_HOLE_SPACING / 2):
        cut = cut + cyl_y(SCREW_PILOT / 2, plate_y0 - 1, plate_y1 + 1, cx + dx, cz)
    return cut


# ----------------------------------------------------------- onderdelen
def chassis():
    """Bodemplaat: vier heupservo's verticaal (as omhoog) op de diagonalen,
    vier afstandsbussen voor het deksel, een buishouder voor de hall-sensor."""
    t = 3.0
    size = 2 * (BODY_HALF + 13) / math.sqrt(2) + 8
    plate = rounded_plate(size, size, 14, t)
    cuts = []
    for k in range(4):
        a = math.radians(45 + 90 * k)
        # servo ligt langs de diagonaal, as op de heup, behuizing naar binnen
        c = BODY_HALF - SERVO_SHAFT_OFS
        win = box(-SERVO_L / 2, SERVO_L / 2, -SERVO_W / 2, SERVO_W / 2, -1, t + 1)
        for dx in (-SERVO_HOLE_SPACING / 2, SERVO_HOLE_SPACING / 2):
            win = win + cyl(SCREW_PILOT / 2, t + 2, -1).translate([dx, 0, 0])
        cuts.append(win.translate([c, 0, 0]).rotate([0, 0, math.degrees(a)]))
    cuts.append(cyl(9, t + 2, -1))                       # kabeldoorvoer midden
    for k in range(4):                                   # sleuven voor kabelbinders
        a = 90 * k
        cuts.append(box(14, 18, -6, 6, -1, t + 1).rotate([0, 0, a]))
    body = plate - union(cuts)
    posts = []
    for k in range(4):
        p = cyl(4, 28, t) - cyl(1.3, 30, t)              # M3 zelftapper
        posts.append(p.translate([36, 0, 0]).rotate([0, 0, 90 * k]))
    # Buisje onder de buik voor de hall-sensor (A3144 / KY-003 zonder printje)
    hall = cyl(5, 40, -40) - cyl(3.2, 42, -41) - box(-1.5, 1.5, -6, 6, -41, -30)
    hall = hall.translate([0, 22, 0])
    return body + union(posts) + hall


def lid():
    """Deksel: rond, met gleuf rondom voor de WS2812B-strip, sleuven voor een
    accubandje, en voor een verticale servo voor het hoofd (pan)."""
    r, t = 46.0, 3.0
    disc = cyl(r, t)
    skirt = cyl(r, 11, -11) - cyl(r - 2.5, 13, -12)
    groove = cyl(r + 1, 10.5, -11) - cyl(r - 1.2, 12, -12)  # strip (10 mm) ligt in de rand
    body = disc + (skirt - groove) + (cyl(r, 1.5, -11) - cyl(r - 2.5, 3, -12))
    cuts = []
    for k in range(4):
        cuts.append(cyl(1.7, t + 2, -1).translate([36, 0, 0]).rotate([0, 0, 90 * k]))
    for y in (-22, 22):                                   # accubandje
        cuts.append(box(-20, 20, y - 2, y + 2, -1, t + 1))
    cuts.append(cyl(6, t + 2, -1).translate([-25, 0, 0]))  # kabels naar boven
    # Pan-servo voor het hoofd: verticaal, as op (34, 0), behuizing naar achteren
    win = box(-SERVO_L / 2, SERVO_L / 2, -SERVO_W / 2, SERVO_W / 2, -1, t + 1)
    for dx in (-SERVO_HOLE_SPACING / 2, SERVO_HOLE_SPACING / 2):
        win = win + cyl(SCREW_PILOT / 2, t + 2, -1).translate([dx, 0, 0])
    cuts.append(win.translate([34 - SERVO_SHAFT_OFS, 0, 0]))
    return body - union(cuts)


def coxa_bracket():
    """Op de heuphoorn (as = z). Draagt de dij-servo met as tangentieel (y) op
    COXA_LEN radiaal naar buiten (+x)."""
    zf = 12.0                       # hoogte dij-as boven onderkant hoornschijf
    cx = COXA_LEN - SERVO_SHAFT_OFS
    hub = cyl(10, 3.5)
    floor = box(0, cx + SERVO_L / 2 + 5, -16, 2.5, 0, 3.5)
    plate = box(-4, cx + SERVO_L / 2 + 5, 0, 3, 0, zf + SERVO_W / 2 + 4)
    rib = box(cx + SERVO_L / 2 + 2, cx + SERVO_L / 2 + 5, -16, 3, 0, zf + SERVO_W / 2 + 4)
    part = hub + floor + plate + rib
    part = part - servo_window(cx, zf, 0, 3) - horn_recess()
    return part


def femur():
    """Dijbeen: hoorn op de oorsprong (as = y), knieservo met as op x = FEMUR_LEN."""
    t = 3.0
    cx = FEMUR_LEN - SERVO_SHAFT_OFS
    xa, xb = -10, cx + SERVO_L / 2 + 5
    plate = box(xa + 10, xb, 0, t, -SERVO_W / 2 - 4, SERVO_W / 2 + 4) + cyl_y(10, 0, t)
    flange = (box(8, cx - SERVO_L / 2 - 1, 0, 9, SERVO_W / 2 + 2, SERVO_W / 2 + 4) +
              box(8, cx - SERVO_L / 2 - 1, 0, 9, -SERVO_W / 2 - 4, -SERVO_W / 2 - 2))
    part = plate + flange
    recess = horn_recess().rotate([90, 0, 0])            # hoorn aan de -y kant (y=0 vlak)
    part = part - servo_window(cx, 0, 0, t) - recess
    return part


def tibia():
    """Scheen: hoorn op de oorsprong (as = y), voet op x = TIBIA_LEN."""
    from manifold3d import CrossSection
    t = 4.0
    prof = CrossSection([[(0, -8), (TIBIA_LEN, -3.5), (TIBIA_LEN, 3.5), (0, 8)]])
    beam = Manifold.extrude(prof, t).rotate([-90, 0, 0]).translate([0, t, 0])  # y: 0..t
    beam = beam + cyl_y(10, 0, t)
    rib = box(10, TIBIA_LEN - 8, 0, 10, -1.25, 1.25)
    foot = Manifold.sphere(5.5, SEG).translate([TIBIA_LEN, t / 2, 0])
    tip = cyl_y(4.5, -2, t + 2, TIBIA_LEN, 0)            # plek voor TPU-dopje
    part = beam + rib + foot + tip
    part = part - horn_recess().rotate([90, 0, 0])
    return part


def head_bracket():
    """Op de pan-hoorn (as = z): U-beugel die de tilt-servo horizontaal draagt."""
    zf = 16.0
    cx = 0.0                                             # tilt-as recht boven pan-as
    hub = cyl(10, 3.5)
    plate = box(cx - SERVO_L / 2 - 5, cx + SERVO_L / 2 + 5, -2, 1, 0, zf + SERVO_W / 2 + 4)
    floor = box(cx - SERVO_L / 2 - 5, cx + SERVO_L / 2 + 5, -2, 14, 0, 3.5)
    part = hub + plate + floor
    part = part - servo_window(cx + SERVO_SHAFT_OFS, zf, -2, 1) - horn_recess()
    return part


def head_shell():
    """Hoofdje met venster voor de 0.91" OLED (printje 38 x 12 mm) en een
    zijwand die op de tilt-hoorn wordt geschroefd. Open achterkant."""
    w, h, d, t = 46.0, 24.0, 20.0, 2.0
    outer = rounded_plate(w, h, 5, d)
    inner = rounded_plate(w - 2 * t, h - 2 * t, 3, d).translate([0, 0, t])
    shell = outer - inner
    window = box(-12, 12, -3.5, 3.5, -1, t + 1).translate([2, 0, 0])   # actief beeld 22.4 x 5.6
    pcb = box(-19.5, 19.5, -6.5, 6.5, t, t + 1.8)                         # printje ligt hierin
    ledges = box(-21, 21, -9, -6.5, t, t + 3) + box(-21, 21, 6.5, 9, t, t + 3)
    shell = shell - window + (ledges - pcb)
    side = box(w / 2 - 0.5, w / 2 + 3, -h / 2, h / 2, 0, d)
    hornside = side - horn_recess().rotate([0, -90, 0]).translate([w / 2 + 3, 0, d / 2])
    return shell + hornside


def dock():
    """Laadplek/thuisbasis: matje met magneet (Ø10 x 3 mm) waar de hall-sensor
    in de buik op reageert. Lage rand als geleiding voor de buik."""
    base = rounded_plate(120, 120, 12, 3)
    rim = rounded_plate(120, 120, 12, 6) - rounded_plate(110, 110, 8, 8).translate([0, 0, -1])
    magnet = cyl(5.2, 3.2, 3 - 3.2 + 0.01).translate([0, 22, 0])
    mark = box(-1, 1, -50, -30, 2.4, 3.1)               # pijl/merkteken voorkant
    return base + rim - magnet - mark


def foot_cap():
    """Voetdopje — bij voorkeur in TPU printen voor grip."""
    return cyl(6.5, 8) - cyl(4.6, 6, 2)


PARTS = {
    # naam: (functie, aantal)
    "chassis": (chassis, 1),
    "lid": (lid, 1),
    "coxa_bracket": (coxa_bracket, 4),
    "femur": (femur, 4),
    "tibia": (tibia, 4),
    "head_bracket": (head_bracket, 1),
    "head_shell": (head_shell, 1),
    "dock": (dock, 1),
    "foot_cap_TPU": (foot_cap, 4),
}


def export(m, path):
    mesh = m.to_mesh()
    tm = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3],
                         faces=np.asarray(mesh.tri_verts), process=False)
    tm.export(path)
    return tm


def main():
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stl")
    os.makedirs(out, exist_ok=True)
    for name, (fn, qty) in PARTS.items():
        m = fn()
        tm = export(m, os.path.join(out, f"{name}.stl"))
        ext = tm.bounds[1] - tm.bounds[0]
        print(f"{name:14s} x{qty}  {ext[0]:6.1f} x {ext[1]:6.1f} x {ext[2]:6.1f} mm  "
              f"volume {m.volume() / 1000:6.1f} cm3  watertight={tm.is_watertight}")


if __name__ == "__main__":
    main()
