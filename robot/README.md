# SPIN — een 3D-geprinte vierpotige WiFi-robot

SPIN is een vierpotige spinrobot (12 beenservo's + 2 voor het hoofd) met een
OLED-gezicht, ledverlichting en een hall-sensor in de buik. De robot draait een
JSON-API over WiFi, zodat hij te besturen is vanaf de webpagina, je telefoon, de
commandoregel, of door Claude (Claude Code op een computer in hetzelfde netwerk).
Je kunt hem ook **nieuwe bewegingen leren**; die blijven bewaard in het flashgeheugen.

```
          [ OLED-hoofd ]        <- pan/tilt (2x MG90S)
        ___/‾‾‾‾‾‾‾‾\___
   \___/  deksel + leds \___/   <- WS2812B in de rand, accu bovenop
   /   \___ chassis ____/   \   <- ESP32 + voeding binnenin
  /      4x heup  (MG90S)    \
 /   4x dij + 4x scheen       \ <- 3 servo's per poot
```

## 0. Al een frame geprint? (PCBWay "Quadruped Spider Robot SG90 / Arduino Nano")

De firmware staat standaard ingesteld op dat veelgebruikte frame (`#define FRAME_SPIDER_SG90 1`
in `firmware/src/config.h`). De printonderdelen in `cad/` heb je dan niet nodig.
Alleen `dock.stl` en eventueel `head_shell.stl` (OLED-houder) zijn handig.

Wat er anders is dan in de originele handleiding:

- **ESP32 in plaats van Arduino Nano.** Gebruik de pintabel hieronder en niet de Nano-print of shield.
  De originele Nano-code (FlexiTimer2) heb je niet nodig.
- **MG90S in plaats van SG90.** Ze hebben dezelfde maat, maar de MG90S is sterker en iets zwaarder.
  Gebruik de hoorns die bij de MG90S horen; de SG90-hoorns passen niet op de as.
- **Hoofdservo's (12, 13) zijn optioneel.** Dit frame heeft geen hoofd. Niet aansluiten is prima.

**Meet je print na** (hart van as tot hart van as, in mm) en zet de waarden in `config.h`:

| Constante | Wat | Standaard |
|---|---|---|
| `COXA_LEN` | heupservo-as → dijservo-as | 27.5 |
| `FEMUR_LEN` | dijservo-as → knieservo-as | 55 |
| `TIBIA_LEN` | knieservo-as → punt van de voet | 77.5 |
| `HIP_SPACING` | afstand tussen twee naburige heupassen | 71 |

**Controleer de 90°-stand** (`python3 tools/spin.py pose neutral`, vóór het monteren
van de hoorns): elke heup wijst **recht opzij** (haaks op de zijkant van het lichaam),
elke dij staat **horizontaal** en elke scheen hangt **recht naar beneden**. Wijst
de heup bij jouw frame op 90° schuin naar de hoek, zet dan `LEG_NEUTRAL_DEG` op
`{45, -45, 135, -135}`.

Poot-nummering, van bovenaf gezien met de voorkant naar voren: 0 = voor-links,
1 = voor-rechts, 2 = achter-links, 3 = achter-rechts.

## 1. Wat gebruik je van je onderdelen

| Onderdeel | Gebruik in SPIN |
|---|---|
| ESP32 (DevKit) | Het brein: WiFi, webserver, looppatroon, 14 servosignalen |
| 20x MG90S | **14 gebruikt** (4 poten x 3 + pan/tilt hoofd). **Bewaar 6 als reserve**: MG90S-tandwielen slijten of breken |
| 0.91" OLED (SSD1306, 128x32, I2C) | Gezicht met ogen (knipperen, blij, boos, ...) en tekst |
| WS2812B-leds | 8 stuks in de rand van het deksel: status en sfeer |
| Hall-sensor | In de buik: herkent de magneet in het `dock` (thuisbasis) |
| D1 mini (ESP-12F) | Niet nodig voor v1. Zie [Ideeën](#9-ideeën-voor-later) |
| 150 kg-servo | **Niet geschikt** voor SPIN: te zwaar (~150-200 g) en vraagt 12-24 V met meerdere ampère. Zie [Ideeën](#9-ideeën-voor-later) |

### Wat je nog nodig hebt (klein spul)

- **Voeding voor de servo's**: 2S LiPo (7,4 V, ≥1500 mAh) of 2x 18650 in serie, **plus een 5-6 V UBEC/step-down van minstens 5 A (liever 8 A)**.
  14 servo's trekken tijdens het lopen samen 2-4 A, met pieken boven 5 A. Een USB-kabel of de 5V-pin van de ESP32 is **veel te zwak**.
- Elco 1000 µF / 10 V over de servovoeding, weerstand 330 Ω in de leddata
- Schakelaar, dupont-kabels of een gaatjesprint met 14x 3-pins header
- M2 zelftappers (servo's en hoorns, meestal meegeleverd), 4x M3x8 zelftappers voor het deksel
- Magneet Ø10 x 3 mm (neodymium) voor het dock
- Optioneel: weerstanden 100 kΩ + 47 kΩ om de accuspanning te meten

## 2. Bedrading

Alle massa's (GND) van accu/UBEC, servo's, ESP32, leds en sensoren **met elkaar verbinden**.

| Functie | ESP32-pin | Opmerking |
|---|---|---|
| Voor-links heup / dij / scheen (servo 0, 1, 2) | 13 / 14 / 27 | |
| Voor-rechts heup / dij / scheen (3, 4, 5) | 16 / 17 / 18 | |
| Achter-links heup / dij / scheen (6, 7, 8) | 25 / 26 / 33 | |
| Achter-rechts heup / dij / scheen (9, 10, 11) | 19 / 23 / 32 | |
| Hoofd pan / tilt (12, 13) | 4 / 5 | |
| OLED SDA / SCL | 21 / 22 | OLED VCC op **3V3** |
| WS2812B DIN | 15 | via 330 Ω; leds-VCC op de 5 V van de UBEC |
| Hall-sensor OUT | 34 | sensor-VCC op **3V3** (zie onder) |
| Accuspanning (optioneel) | 35 | accu+ → 100 kΩ → pin 35 → 47 kΩ → GND |

Voeding:

```
Accu 7,4V ──[schakelaar]──┬── UBEC in          UBEC 5-6V uit ──┬── alle servo's (rood)
                          │                                     ├── WS2812B 5V
                          └── (optioneel deler → pin 35)        ├── ESP32 VIN/5V
                                                                └── 1000 µF elco
Alle GND samen ─────────────────────────────────────────────────────────────────────
```

**Let op bij de hall-sensor:** een KY-003-module op 5 V geeft 5 V terug op de
signaalpin, en dat kan de ESP32 beschadigen. Voed hem met 3,3 V (de meeste
modules werken prima), of zet een spanningsdeler in de signaallijn. Geeft je
module 0 bij een magneet en 4095 zonder, dan klopt `HALL_ACTIVE_LOW = true`.
Een analoge sensor (49E) werkt ook: kijk naar `hall.raw` in de status en pas de
drempels in `serviceSensors()` aan.

## 3. Printen (eigen SPIN-onderdelen)

> Heb je het PCBWay-spinframe al geprint? Dan kun je dit hoofdstuk overslaan (zie hoofdstuk 0).

```bash
cd robot/cad
pip install manifold3d trimesh numpy
python3 spin_parts.py        # schrijft alle STL's naar robot/cad/stl/
```

De STL's staan al in `cad/stl/`. Aantallen:

| Bestand | Aantal | Tip |
|---|---|---|
| `chassis.stl` | 1 | plat op het bed, het hall-buisje wijst naar beneden: met support |
| `lid.stl` | 1 | ondersteboven printen (platte kant op het bed) |
| `coxa_bracket.stl` | 4 | **print er eerst één** en controleer of de servo past |
| `femur.stl` | 4 | op de zijkant |
| `tibia.stl` | 4 | op de zijkant |
| `head_bracket.stl` | 1 | |
| `head_shell.stl` | 1 | met de voorkant op het bed |
| `dock.stl` | 1 | magneet erin lijmen, vlak met de bovenkant |
| `foot_cap_TPU.stl` | 4 | in TPU voor grip (of overslaan) |

PLA of PETG, 0,2 mm laagdikte, 3 wanden, 25-30 % infill. Past een servo te
strak of te los? Pas `SERVO_L`/`SERVO_W` (of de hoornmaten) bovenaan
`spin_parts.py` aan en genereer opnieuw.

## 4. Montage (eigen SPIN-onderdelen; bij het PCBWay-frame: volg die handleiding, maar met de pinnen hierboven)

1. **Zet alle servo's eerst op 90°** voordat je een hoorn monteert: flash de
   firmware, sluit de servo's aan en stuur `python3 tools/spin.py pose neutral`.
   Zet dan pas de hoorns erop.
2. Heupservo's van onderen door het chassis, as omhoog, lipjes op de plaat.
3. Op 90° hoort elke `coxa_bracket` **recht naar buiten langs de diagonaal** te wijzen.
4. Dijservo in de bracket; het `femur` op 90° **horizontaal**.
5. Knieservo in het femur; de `tibia` op 90° **haaks op het femur** (recht naar beneden).
6. ESP32, UBEC en bekabeling in het chassis, deksel erop (4x M3), accu met een
   klittenband door de sleuven op het deksel.
7. Pan-servo verticaal in het deksel (voorkant), `head_bracket` erop, tilt-servo
   in de beugel, `head_shell` met de OLED op de tilt-hoorn.
8. Ledstrip (8 leds) in de gleuf rondom het deksel. Hall-sensor in het buisje onder de buik.

## 5. Firmware flashen

Met [PlatformIO](https://platformio.org) (VS Code-extensie of `pip install platformio`):

```bash
cd robot/firmware
cp src/secrets.example.h src/secrets.h   # vul je WiFi-naam en wachtwoord in
pio run -t upload
pio device monitor                       # toont het IP-adres
```

Kan de robot geen verbinding maken, dan start hij een eigen hotspot
**SPIN-robot** (wachtwoord `spinrobot`); open dan http://192.168.4.1.
Volgende updates kunnen draadloos: `pio run -e ota -t upload`.

## 6. Kalibreren

Hobbyservo's staan nooit precies goed. Per servo kun je een offset (±30°) en de
draairichting instellen, zonder opnieuw te flashen:

```bash
python3 tools/spin.py pose neutral          # alles op 90°
python3 tools/spin.py calibrate 4 -6        # servo 4 zes graden bijstellen en opslaan
python3 tools/spin.py raw '{"cmd":"calibrate","id":7,"dir":-1,"save":true}'   # richting omdraaien
python3 tools/spin.py pose stand
```

Controle van de richting: stuur een dij-servo naar 120 (`servo 1 120`).
**De poot moet omhoog gaan.** Gaat hij omlaag, draai dan de richting om (`dir: -1`).
Een scheen op 120 hoort naar buiten te bewegen, en een heup op 120 naar
links/tegen de klok in (van bovenaf gezien).

## 7. Besturen

- **Webpagina:** http://spin.local (of het IP-adres): lopen, houdingen, gezicht,
  leds, sliders per servo en bewegingen aanleren.
- **Commandoregel:** `tools/spin.py` (alleen standaard Python nodig):

```bash
export SPIN_HOST=spin.local
python3 tools/spin.py walk forward 4
python3 tools/spin.py walk turn_left 2 --speed 1.4
python3 tools/spin.py play wave
python3 tools/spin.py face love
python3 tools/spin.py say "Hallo!"
python3 tools/spin.py led rainbow
python3 tools/spin.py status
python3 tools/spin.py stop
```

- **API:** `POST /api/cmd` met JSON; de volledige lijst staat op `GET /api/help`
  (ook in `firmware/src/web_ui.h`). Stuur een lijst commando's om een reeks af te
  spelen:

```bash
curl -X POST http://spin.local/api/cmd -d '[
  {"cmd":"face","expr":"happy"},
  {"cmd":"walk","dir":"forward","steps":3},
  {"cmd":"walk","dir":"turn_left","steps":2},
  {"cmd":"play","name":"bow"},
  {"cmd":"led","mode":"rainbow"}
]'
```

### Bewegingen aanleren

Een move is een reeks frames: per frame 14 servohoeken (of `null` = niet
veranderen) en een duur in ms. Aanleren kan via de webpagina (sliders zetten,
**+ frame**, naam invullen, **opslaan**) of via de API:

```bash
curl -X POST http://spin.local/api/cmd -d '{"cmd":"save_move","name":"knik","frames":[
  {"ms":300,"a":[null,null,null,null,null,null,null,null,null,null,null,null,90,70]},
  {"ms":300,"a":[null,null,null,null,null,null,null,null,null,null,null,null,90,110]},
  {"ms":300,"a":[null,null,null,null,null,null,null,null,null,null,null,null,90,90]}]}'
python3 tools/spin.py play knik 3
```

Ingebouwd: `wave`, `hello`, `bow`, `dance`, `shake`, `stretch`, `look_around`.

## 8. Claude laten besturen

Claude stuurt de robot aan via dezelfde API. Daarvoor moet Claude het
netwerk van de robot kunnen bereiken:

1. Installeer [Claude Code](https://claude.com/claude-code) op een computer **in
   hetzelfde WiFi-netwerk** als de robot.
2. Open deze repository in een terminal en start `claude`.
3. Vraag bijvoorbeeld: *"Laat SPIN drie stappen vooruit lopen, rondkijken en zwaaien"*
   of *"Leer SPIN een nieuw dansje"*.

`robot/CLAUDE.md` vertelt Claude hoe de API werkt en welke veiligheidsregels gelden.
Claude leest `/api/status` (gebeurtenissen, hall-sensor, accuspanning, servohoeken)
om te zien wat de robot doet.

> Een Claude-sessie in de cloud (claude.ai/code) kan je thuisnetwerk niet bereiken.
> Zet de robot niet zomaar open op internet. Als je dat toch wilt: stel eerst een
> `API_KEY` in `secrets.h` in en gebruik een beveiligde tunnel.

## 9. Ideeën voor later

- **D1 mini:** een tweede, kleinere robot, een draadloze afstandsbediening (met
  een paar knoppen of een joystick die `/api/cmd` aanroept), of een slim dock met
  eigen leds dat meldt wanneer SPIN thuis is.
- **150 kg-servo:** een eigen project, zoals een robotarm-basis of een draaiplateau,
  met een eigen 12 V-voeding. Hij kan dan via dezelfde API-stijl op een tweede ESP aangestuurd worden.
- **Extra MG90S:** een grijper of staart (er zijn op de ESP32 nog vrije pinnen: 2 en 12).
- Afstandssensor (VL53L0X, I2C) in het hoofd, zodat SPIN obstakels ziet.

## Mappen

```
robot/
├── firmware/        PlatformIO-project voor de ESP32
│   └── src/         config.h (pinnen, maten), kinematics.h (IK), motion.h,
│                    face.h (OLED + leds), web_ui.h, main.cpp
├── cad/             spin_parts.py + stl/
├── tools/spin.py    bediening vanaf de commandoregel
└── CLAUDE.md        instructies voor Claude
```

**Status:** versie 1, nog niet op echte hardware getest. De beenwiskunde is
gecontroleerd met een test (inverse en voorwaartse kinematica komen overeen, en
alle looppatronen blijven binnen het servobereik). Verwacht bij de eerste opbouw
wat bijstellen: kalibratie-offsets, draairichtingen en de pasvorm van de prints.
