# Platine: M18-Brücke mit ESP32-S3 SuperMini

Eigene Leiterplatte für die [Akku-Brücke](../../esp32-bridge/README.md). Die vier Akkukontakte sitzen direkt auf der Platine,
der ESP32-S3 SuperMini steckt auf Buchsenleisten. Der Pegelwandler ist durch zwei kleine Transistorstufen ersetzt, der
Step-down-Wandler durch einen RECOM-Schaltregler. Alles ist von Hand lötbar (SMD ab 0805 / SOT-23 / SMA).

| Oben | Unten (Akkuseite) |
|---|---|
| ![Platine oben](bilder/platine-oben.png) | ![Platine unten](bilder/platine-unten.png) |

![3D-Ansicht](bilder/platine-3d.png)

> **Stellen mit ⚠ PRÜFEN** beruhen auf Annahmen, weil Datenblatt, Pinbelegung oder Foto beim Entwurf fehlten.
> Bitte vor der Bestellung der 20 Platinen kontrollieren (Liste ganz unten).

## Eckdaten

| | |
|---|---|
| Größe | **58 × 38 mm**, 2 Lagen, 1,6 mm, 35 µm Kupfer |
| Befestigung | 4 × **M3** (Bohrung 3,2 mm), Mitte 4 mm von den Kanten, Lochabstand **50 × 30 mm** |
| Akkukontakte | 4 × TE **928814-1** (FASTON 6,35 × 0,81 mm) auf der **Unterseite**, Messerspitzen bündig mit der linken Kante |
| Versorgung | aus dem Akku (B+ 15–21 V) oder über USB-C des SuperMini |
| Ruhestrom aus B+ | nur der Eigenverbrauch des Reglers (R-78E, Wert im Datenblatt nachsehen und am Muster messen) und der Schlafstrom des ESP. Die J2-Stufe zieht im Ruhezustand nichts. |
| Laden | vorbereitet: Lötlöcher CHG+/CHG− für 1,5-mm²-Litze und 6-poliger Steuerstecker, nichts bestückt |

Bezugspunkt aller Maße ist die linke obere Ecke der Draufsicht (x nach rechts = Einsteckrichtung, y nach unten).

## Akkukontakte

Draufsicht, Messerspitzen links (zeigen zum Akku hin):

| Kontakt | Bauteil | Mitte y | Abstand zum Nachbarn |
|---|---|---|---|
| **B−** | X4 | 8,45 mm | – |
| **J2** | X3 | 13,80 mm | 5,35 mm |
| **J1** | X2 | 24,30 mm | 10,50 mm |
| **B+** | X1 | 29,55 mm | 5,25 mm |

- Von oben in Einsteckrichtung gesehen, Spitzen nach vorne: **links B+, J1, J2, rechts B−**.
- Die Abstände sind aus dem bisherigen Gehäuse übernommen, also aus den Schlitzen im STL des Printables-Modells „Milwaukee M18 All Pins“.
  Gemessen wurden Schlitzmitten bei −10,55 / −5,20 / 0 / +5,30 / +10,55 mm. Der mittlere Schlitz bleibt frei.
- Die Lötstifte liegen bei x = 12,18 und 17,26 mm, das Messer reicht von x = 0,7 bis 16,7 mm. Es ist 6,35 mm hoch und steht senkrecht auf der Unterseite.
- Unter den Messern ist auf der Unterseite keine Kupferfläche (Sperrflächen), damit kein Messer auf einer Massefläche aufliegt.
  Oben über den Messern sitzen nur SMD-Teile, die Unterseite bleibt dort glatt.
- ⚠ **PRÜFEN:** Ohne TE-Datenblatt stammen Lochbild und 3D-Modell aus dem SamacSys-Modell in `quellen/LIB_928814-1`.
  Höhe und Lage des Messers am ersten Muster mit dem Akku ausprobieren, bevor das Gehäuse fertig konstruiert wird.

## Pinbelegung ESP32-S3 SuperMini

Draufsicht auf den SuperMini, USB-C zeigt nach rechts (Platinenkante). Reihenabstand 15,24 mm, je 9 Pins.

| Pin | Obere Reihe (y = 11,38) | Funktion | Untere Reihe (y = 26,62) | Funktion |
|---|---|---|---|---|
| 1 / 10 (USB-Seite) | GPIO43 (U0TXD) | frei | 5V | Versorgung über D3 (B140) aus dem Regler |
| 2 / 11 | GPIO44 (U0RXD) | frei | GND | Masse = B− |
| 3 / 12 | GPIO1 | frei | 3V3 | Versorgung OLED, Pull-ups, Steuerstecker |
| 4 / 13 | GPIO2 | frei | GPIO13 | RGB-LED **rot** (über Q5) |
| 5 / 14 | GPIO3 (Strapping) | frei | GPIO12 | RGB-LED **grün** (über Q6) |
| 6 / 15 | GPIO4 | **Reserve** → Steuerstecker Pin 6 | GPIO11 | RGB-LED **blau** (über Q7) |
| 7 / 16 | GPIO5 | **CHG_EN** → Steuerstecker Pin 5 | GPIO10 | frei |
| 8 / 17 | GPIO6 | **RX** ← J1-Stufe | GPIO9 | **SCL** (4,7 kΩ Pull-up) |
| 9 / 18 | GPIO7 | **TX** → J2-Stufe | GPIO8 | **SDA** (4,7 kΩ Pull-up) |

- Onboard-WS2812 des SuperMini: GPIO48. BOOT-Taste: GPIO0.
- ⚠ **PRÜFEN:** Die Pinbelegung stammt von [espboards.dev](https://espboards.dev/esp32/esp32-s3-super-mini/), weil die Unterlagen
  in `quellen/` noch fehlten. Den Reihenabstand von 15,24 mm (Board 18 mm breit) habe ich angenommen. Vor dem Bestellen
  am eigenen Board nachmessen bzw. mit dem Aufdruck vergleichen.

## Steckverbinder und Lötpunkte

| Bauteil | Pin 1 → n | Hinweis |
|---|---|---|
| **J1** OLED, Buchse 1×4 (Harwin M20-7820442) | **GND, VCC (3V3), SCL, SDA** | Pin 1 ist der rechte Pin (eckiges Pad). ⚠ **PRÜFEN:** Reihenfolge ohne Foto angenommen, die meisten 0,96″-SSD1306-Module sind so belegt. |
| **J2** Lade-Steuerung, Stiftleiste 1×6 | 3V3, GND, SDA, SCL, CHG_EN, Reserve (GPIO4) | **nicht bestücken** (DNP) |
| **TP1** CHG+ | B+ direkt am Akkukontakt, **vor** der Sicherung | Lötloch 1,8 mm, für ~5 A |
| **TP2** CHG− | B− direkt am Akkukontakt | Lötloch 1,8 mm, für ~5 A |

Die Ladeleitungen sind breit ausgeführt:
- **B+ → CHG+:** 2,5 mm auf beiden Lagen.
- **B− → CHG−:** 3 mm auf der Unterseite plus Massefläche.

Der Ladestrom fließt **nicht** durch F1. Eine spätere Ladeplatine braucht deshalb ihre eigene Absicherung.

## Schaltung

Schaltplan: [`m18-bruecke.kicad_sch`](m18-bruecke.kicad_sch), als PDF in [`fertigung/m18-bruecke-schaltplan.pdf`](fertigung/m18-bruecke-schaltplan.pdf).

**Versorgung**
- **Pfad:** B+ → **F1** 500 mA träge (Littelfuse 0452.500MRL) → **D1** B1100 (Verpolschutz) → VBAT.
- **Schutz und Puffer an VBAT:** TVS **D2** SMBJ24A und **C1** 4,7 µF/50 V.
- **Regler:** **U2** R-78E5.0-0.5 macht aus VBAT 5 V (C2 10 µF). **D3** B140 führt die 5 V zum 5V-Pin des SuperMini.
- **USB und Regler gleichzeitig:** D3 verhindert, dass USB in den Regler zurückspeist. Der Regler liefert hinter D3 nur ~4,6 V und
  liegt damit unter der USB-Spannung, er speist also auch nicht in den PC. Das Arbeiten gegeneinander, das beim DevKit die Lötbrücke „IN-OUT“
  verursacht hat, fällt weg.

**J2 – Senden** (Q1 MMBT3904, Q2 MMBT3906, nicht invertierend: TX high → J2 = B+)
- **Ansteuerung:** GPIO7 → R1 2,2 kΩ → Basis Q1. **R2 4,7 kΩ** zieht die Basis nach GND. Auch mit dem internen Pull-up des ESP
  beim Booten bleibt die Basis unter 0,3 V, J2 bleibt also low.
- **High-Side-Schalter:** Q1 schaltet über R4 10 kΩ die Basis von Q2. R3 10 kΩ hält Q2 gesperrt. Q2 legt B+ (VBAT) über **R5 1 kΩ** an J2.
- **Low-Pegel:** **R6 10 kΩ** zieht J2 nach B−, wenn Q2 aus ist.
- **Strom:** Im Ruhezustand fließt kein Strom aus B+. Bei J2 = high sind es ca. 4 mA, davon 2 mA in die Basis von Q2 und ~2 mA durch R6.
- Die Firmware bleibt gleich: Der UART-TX ist im Ruhezustand invertiert (= low), genau wie mit dem Pegelwandler.

**J1 – Empfangen** (Q3, Q4 MMBT3904 wie im Spud-Isolator `docs/spud_isolator.png`, nicht invertierend)
- J1 → R7 15 kΩ → Basis Q3, R8 100 kΩ nach GND (nur damit ein offener Kontakt nicht schwebt).
- Q3 zieht die Basis von Q4 auf Low, R9 15 kΩ Pull-up auf 3,3 V. Q4 zieht RX (GPIO6) auf Low, R10 10 kΩ Pull-up auf 3,3 V.

**RGB-LED** (Dialight 521-8559F, 5 mm, gemeinsame Anode)
- Grün und Blau brauchen ~3,2 V und gehen deshalb nicht an 3,3 V. Die Anode liegt an +5 V, jede Kathode an einem MMBT3904 (Q5–Q7, Basis 2,2 kΩ).
- Vorwiderstände: rot 270 Ω, grün/blau 150 Ω, das ergibt ca. 7–9 mA bei voller Helligkeit.
- Die Firmware zeigt auf der LED dieselben Farben wie auf der WS2812.
- Hinweis: Die Basen von Q5–Q7 haben keinen Pull-down. Beim Einschalten kann die LED kurz schwach glimmen, bis die Firmware die Pins setzt.
  Im Tiefschlaf hält die Firmware die Pins auf Low.

## Firmware

Variante `BOARD_SUPERMINI` (TX = GPIO7, RX = GPIO6, RGB-LED an GPIO13/12/11, CHG_EN = GPIO5 low, 4 MB Flash, PSRAM quad),
siehe [esp32-bridge/README.md](../../esp32-bridge/README.md#variante-eigene-platine-mit-esp32-s3-supermini).
Im [Web-Flasher](https://danunt97.github.io/m18-protocol/) gibt es dafür einen eigenen Knopf „ESP32-S3 SuperMini“.

## Fertigung

| Datei | Inhalt |
|---|---|
| [`fertigung/m18-bruecke-gerber.zip`](fertigung/m18-bruecke-gerber.zip) | Gerber (X2) und Bohrdateien (Excellon, mm, PTH/NPTH getrennt) – so beim Leiterplattenhersteller hochladen |
| `fertigung/gerber/` | dieselben Dateien ausgepackt, dazu die Bohrpläne als PDF |
| [`fertigung/m18-bruecke-bestellliste.xlsx`](fertigung/m18-bruecke-bestellliste.xlsx) | Stückliste für 20 Platinen mit Hersteller- und Mouser-Nummern und Links |
| `fertigung/m18-bruecke-bestueckung-oben.pdf` / `-unten.pdf` | Bestückungsplan (alle Referenzen, auch die, die im Bestückungsdruck keinen Platz haben) |
| `fertigung/m18-bruecke-schaltplan.pdf` | Schaltplan |

Einstellungen beim Hersteller: 2 Lagen, 1,6 mm FR4, 1 oz Kupfer, HASL oder ENIG. Minimale Werte im Design: Leiterbahn 0,3 mm,
Abstand 0,2 mm, Via 0,7/0,35 mm, Kupfer zum Rand 0,4 mm.

**Prüfung:** ERC 0 Fehler/Warnungen. DRC (alle Schweregrade) 0 Verstöße, 0 offene Verbindungen, Schaltplan und Platine stimmen überein.

### Bestücken

1. SMD-Teile oben (Widerstände, Transistoren, Dioden, C1/C2, F1). Die Teile unter dem SuperMini zuerst, die Buchsenleisten kommen danach.
2. R-78E (U2), Buchsenleisten für den SuperMini (2 × 1×9) und die OLED-Buchse.
3. RGB-LED: Die Pads sind versetzt. Die vier Beinchen leicht abwechselnd nach vorne und hinten biegen. Der lange Anschluss (Anode) gehört an Pin 2.
4. Zuletzt die vier Akkukontakte von **unten** einsetzen und oben verlöten. Die Messer müssen senkrecht stehen.
5. **Ohne SuperMini** an ein Netzteil (12–20 V an B+/B−, Strom auf 100 mA begrenzen) und am 5V-Pin des Sockels ~4,6 V messen.
6. SuperMini mit Firmware einstecken. Mit Akku prüfen: **J2 gegen B− unter 1 V**.

## ⚠ Vor der Bestellung prüfen

1. **SuperMini-Pinbelegung und Reihenabstand** (15,24 mm) am echten Board, siehe oben.
2. **OLED-Pinreihenfolge** GND-VCC-SCL-SDA am eigenen Display.
3. **Kontaktreihenfolge**: links B+, J1, J2, rechts B−, von oben in Einsteckrichtung gesehen. Sonst sind B+ und B− vertauscht.
   D1 schützt dann zwar die Elektronik, die Brücke funktioniert aber nicht.
4. **TE 928814-1**: Datenblatt fehlte, Maße aus dem SamacSys-Modell. Lochdurchmesser 1,45 mm und Messerhöhe prüfen.
5. **Mouser-Nummern, Preise, Lager**: in der Bestellliste über die Links gegenprüfen. Die Links suchen nach der Herstellernummer.
6. **J2-Pegel**: R6 10 kΩ zieht J2 im Ruhezustand nach B−. Falls der Akku J2 intern stark nach oben zieht, R6 verkleinern (z. B. 4,7 kΩ). Am Muster messen.
7. **Höhe**: R-78E ~10 mm, Buchsenleisten 8,5 mm plus SuperMini, RGB-LED 8,6 mm. Für das neue Gehäuse aus dem 3D-Modell (`m18-bruecke.kicad_pcb`, 3D-Ansicht) übernehmen.

## Dateien

| Datei | Inhalt |
|---|---|
| `m18-bruecke.kicad_pro/.kicad_sch/.kicad_pcb` | KiCad-10-Projekt |
| `lib/m18.kicad_sym`, `lib/m18.pretty`, `lib/3d` | Projektbibliothek: SuperMini-Sockel, TE 928814-1, Harwin-Buchse, RGB-LED (KiCad-Footprint ohne den Silk-Bogen an Pad 4), Lötloch |
| `quellen/` | Herstellerbibliotheken (TE 928814-1, Dialight 521-8559F, Harwin M20-7820442) |
| `tools/` | Skripte, mit denen die Platine entstanden ist (KiCad-Python): `platzieren.py` + `lage.py` (Bauteillage), `vorrouten.py` (breite Akkuleitungen, gesperrt), `flaechen.py` (GND-Flächen, Sperrflächen unter den Messern), `beschriftung.py` (Bestückungsdruck), `abgleich.py` (Platine an Schaltplan angleichen), `bestellliste.py` (Excel-Liste) |

Geroutet wurde mit [Freerouting](https://github.com/freerouting/freerouting) 2.2.4. Die Akkuleitungen sind von Hand gelegt und gesperrt.
Wer in KiCad weiterarbeitet, braucht die Skripte nicht.
