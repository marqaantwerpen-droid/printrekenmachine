# SPIN-robot besturen (instructies voor Claude)

SPIN is een vierpotige robot op een ESP32 in het lokale netwerk van de gebruiker.
Bediening via HTTP/JSON op `http://$SPIN_HOST` (standaard `spin.local`).

## Werkwijze
- Begin met `python3 robot/tools/spin.py status`. Is de robot niet bereikbaar,
  vraag dan naar het IP-adres (staat op de OLED na het opstarten) en gebruik `--host <ip>`.
- `GET /api/help` (`spin.py help`) geeft de actuele lijst commando's. Lees die bij twijfel.
- Bewegingscommando's gaan in een wachtrij. Gebruik `--wait` of poll `status`
  (`busy: false`) voordat je een conclusie trekt over wat er gebeurde.
- Kijk in `status.events` voor wat de robot meldde (fouten, hall-sensor, geleerde moves)
  en in `status.vbat` voor de accu.

## Veiligheid
- Stuur bij iets onverwachts (ongewone hoeken, fouten, de gebruiker zegt "stop") meteen `spin.py stop`.
- Begin rustig: weinig stappen (≤4) en `speed` ≤ 1.0 tot de gebruiker bevestigt dat de robot goed loopt.
- Ruwe servocommando's (`servo`, `save_move`) blijven tussen 10 en 170 graden. Beweeg
  een poot niet zo dat hij door het lichaam of een andere poot gaat; test nieuwe moves
  eerst met lange frames (≥400 ms).
- Wijzig kalibratie (`calibrate`, `calibrate_reset`) alleen als de gebruiker daarom vraagt.
- Is `vbat` onder 6,6 V (2S-accu): zeg dat tegen de gebruiker en stuur `relax`.
- Deze robot staat in iemands huis: beweeg alleen als de gebruiker dat vraagt.

## Nieuwe moves leren
Een move bestaat uit frames `{"ms": duur, "a": [14 hoeken of null]}`. Indexen: 0-2
voor-links (heup, dij, scheen), 3-5 voor-rechts, 6-8 achter-links, 9-11 achter-rechts,
12 hoofd pan, 13 hoofd tilt. 90 is neutraal: dij 90 = horizontaal, hoger getal = poot omhoog.
Lees eerst `status.angles` van de huidige stand als uitgangspunt. Sla op met
`save_move`, speel af met `play`, en vraag de gebruiker hoe het eruitzag.
