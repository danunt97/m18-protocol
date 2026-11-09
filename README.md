# M18 Akku-Verwaltung

[![Tests](https://github.com/danunt97/m18-protocol/actions/workflows/tests.yml/badge.svg)](https://github.com/danunt97/m18-protocol/actions/workflows/tests.yml)
![Python](https://img.shields.io/badge/Python-3.11%2B-3776AB?logo=python&logoColor=white)
![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Arduino-E7352C?logo=espressif&logoColor=white)
![Ohne Abhängigkeiten](https://img.shields.io/badge/Abh%C3%A4ngigkeiten-nur%20pyserial-informational)

Mit diesem Tool **liest du Milwaukee-M18-Akkus aus, verwaltest sie in einer Datenbank und verfolgst ihre Alterung**.
Das Programm läuft als lokaler Webserver auf dem eigenen PC, ohne Installation und ohne Cloud. Auf Wunsch übernimmt
eine **ESP32-S3-Brücke** das Auslesen per WLAN oder direkt vor Ort mit dem Handy.

Das Projekt baut auf der Protokoll-Forschung von [mnh-jansson/m18-protocol](https://github.com/mnh-jansson/m18-protocol) auf.

![Flottenübersicht](docs/images/gui-flotte.png)

## Funktionen

**Akku-Verwaltung (PC, `gui/`)**
- **Auslesen** über einen 3,3-V-USB-Seriell-Adapter oder die ESP32-Brücke. Bekannte Akkus erkennt das Programm automatisch, neue legt es mit einem Klick an.
- **Datenbank (SQLite):** jede Messung mit Zeitstempel, allen Zählern und den rohen Registerwerten.
- **Flottenübersicht:** Zustand, Verlauf, Trend, Zell-Imbalance, Zyklen, Standort, fällige Messungen und offene Warnungen. Die Liste lässt sich filtern und sortieren.
- **Alterung:** Diagramme pro Akku und eine Prognose, wann der Akku schwach wird.
- **Warnungen**, wenn sich ein Akku deutlich verschlechtert: Zustandsabfall, steigende Imbalance, neue Überhitzungs- oder Unterspannungs-Ereignisse, Ladung mit einer Zelle unter 2,5 V.
- **Standort, Kommentar und Gehäuse-Seriennummer** sind editierbar; Akkus lassen sich ausmustern.
- **QR-Etiketten** für P-touch (12–36 mm) und Brother QL (62 mm). Der QR-Code führt direkt zur Akku-Seite, der Code wird offline erzeugt.
- **Berichte:** druckbare A4-Seite bzw. PDF pro Akku, CSV pro Akku oder für die ganze Flotte, Datenbank-Backup.
- **Demo-Modus** mit 8 simulierten Akkus und 2 Jahren Verlauf, ganz ohne Hardware.

**ESP32-S3-Akku-Brücke (`esp32-bridge/`)**
- **Ersetzt den USB-Seriell-Adapter.** Der PC verbindet sich per USB oder WLAN und findet die Brücke im Netz automatisch.
- **Eigene Weboberfläche fürs Handy:**
  - Akku direkt auslesen, mit Zustand, Zellspannungen, Zählern und Daten
  - WLAN-Einrichtung beim Kunden (Captive Portal)
  - Verlauf, Status-LED, Firmware-Update im Browser
- **Messungen offline speichern** (mehrere tausend im Flash) und später **in die PC-Datenbank übernehmen**. Dafür werden die Rohregister gespeichert; eine übernommene Messung ist gleichwertig mit einer am PC.
- **Robust:**
  - Wiederholungen und schneller Abbruch bei gezogenem Akku
  - Watchdog
  - J2 bleibt im Ruhezustand sicher auf Low, damit der Akku keine „Dumb Charge“ zählt
- **Optionaler Passwortschutz.** Einstellungen ändern geht nur nach einem Druck auf die BOOT-Taste.
- **Aufsteckgehäuse zum 3D-Drucken**, Versorgung aus dem Akku, **Energiesparmodus** (Tiefschlaf nach Ruhezeit).
- **Ergebnis auf der LED:** Nach jedem Auslesen leuchtet sie 30 s grün, gelb oder rot, auch wenn der PC ausliest.
- **Firmware im Browser installieren** (ESP Web Tools), ganz ohne Arduino IDE.
- **Cloud (Supabase):** Konto, „Meine Akkus“ mit Verlauf. Jede Messung landet komplett in der Cloud, aus der App oder direkt von der Brücke, wenn sie im WLAN Internet hat.
- **Bluetooth fürs Handy:** Die [Akku-App](https://danunt97.github.io/m18-protocol/app/) läuft im Browser (Android: Chrome, iPhone: kostenlose App „Bluefy“).
  Ohne WLAN-Wechsel: Akku auslesen, alle Werte, Verlauf und Protokoll der Brücke. Keine App-Store-App und kein Entwicklerkonto nötig.

**Betrieb auf dem NAS:** Docker-Image mit **automatischem Abgleich**. Die Messungen der Brücken landen von selbst in der Datenbank.

## Bilder

| Akku-Detail mit Alterungsverlauf | Auslesen und Vergleich mit der letzten Messung |
|---|---|
| ![Akku-Detail](docs/images/gui-akku-detail.png) | ![Auslesen](docs/images/gui-auslesen.png) |
| **QR-Etiketten (P-touch 24 mm)** | **Druckbarer Bericht** |
| ![Etiketten](docs/images/gui-etiketten.png) | ![Bericht](docs/images/gui-bericht.png) |

**ESP32-Brücke am Handy:** WLAN einrichten, Akku auslesen, gespeicherte Messungen

![ESP32-Brücke am Handy](docs/images/esp-handy.png)

**Messungen der Brücke in die PC-Datenbank übernehmen**

![Übernehmen](docs/images/gui-bruecke-uebernehmen.png)

> Alle Bilder stammen aus dem Demo-Modus bzw. aus dem Simulator. Die Messwerte sind simuliert.

## Schnellstart

Du brauchst **Python 3.11 oder neuer**. Weitere Pakete braucht die Demo nicht.

```bash
git clone https://github.com/danunt97/m18-protocol.git
```

```bash
cd m18-protocol
```

```bash
python gui/m18_gui.py --demo
```

Danach öffnet sich der Browser mit http://127.0.0.1:8018. Unter Windows startest du mit `py gui\m18_gui.py --demo`.

**Mit echtem Akku:**

```bash
pip install pyserial
```

```bash
python gui/m18_gui.py
```

Dann **Auslesen** → Adapter wählen → **Verbinden** → **Akku auslesen** → speichern.
Die Datenbank liegt in `data/m18_fleet.db`.

Alle Optionen (`--host 0.0.0.0` für Handy-Scans im WLAN, `--db`, `--port` …) stehen in [gui/README.md](gui/README.md).

**Dauerhaft auf dem NAS / Server (Docker):**

```bash
docker compose up -d
```

Dann ist die Oberfläche unter `http://<NAS-IP>:8018` erreichbar. Die Datenbank liegt in `./data`, und Messungen der ESP32-Brücken
werden **automatisch alle 5 Minuten übernommen**. Das Image (`ghcr.io/danunt97/m18-protocol`) gibt es für amd64 und arm64;
Details stehen in [gui/README.md](gui/README.md#docker).

## Hardware

Es gibt zwei Möglichkeiten, den Akku anzuschließen:

![Übersicht](docs/images/architektur.svg)

1. **3,3-V-USB-Seriell-Adapter** wie im ursprünglichen Projekt. Verdrahtung und Fehlersuche stehen in [docs/m18-protocol.md](docs/m18-protocol.md).
2. **ESP32-S3 (N16R8) als Brücke.** Das bringt WLAN, Messen ohne PC und eine eigene Weboberfläche. Am bequemsten ist das
   **3D-gedruckte Aufsteckgehäuse**: Es wird wie ein Werkzeug auf den Akku geschoben und versorgt die Elektronik über einen
   Step-down-Wandler aus dem Akku. Druckdateien, Stückliste und Zusammenbau stehen in [hardware/aufsteckgehaeuse/](hardware/aufsteckgehaeuse/README.md).

| Aufsteckgehäuse | Elektronik im Gehäuse |
|---|---|
| ![Aufsteckgehäuse](hardware/aufsteckgehaeuse/bilder/gehaeuse-geschlossen.png) | ![Elektronik](hardware/aufsteckgehaeuse/bilder/gehaeuse-elektronik.png) |

![Verdrahtung ESP32-S3](docs/images/verdrahtung-esp32s3.svg)

**Firmware ohne Arduino IDE installieren:** Die Seite **[danunt97.github.io/m18-protocol](https://danunt97.github.io/m18-protocol/)**
öffnen (Chrome oder Edge), das Board per USB anstecken und auf **Installieren** klicken.

Oder mit der Arduino IDE 2 flashen. Ins Werkzeuge-Menü gehören diese Einstellungen:
- Board: **ESP32S3 Dev Module**
- Flash: **16MB**
- PSRAM: **OPI**
- Partition: **16M Flash (3MB APP/9.9MB FATFS)**
- USB CDC On Boot: **Enabled**

Danach `esp32-bridge/m18_bridge/m18_bridge.ino` hochladen. Alles Weitere zu WLAN-Einrichtung, LED, Passwort und Update
steht in [esp32-bridge/README.md](esp32-bridge/README.md).

> ⚠️ **Sicherung (T 500 mA) direkt hinter B+ einbauen** und den **Step-down vor dem ersten Anschließen auf 5,0 V einstellen.** Solange das Gehäuse steckt, versorgt der Akku die Elektronik.
> Die Brücke schläft deshalb nach 30 min Ruhe (BOOT-Taste weckt sie); nach Gebrauch abziehen.
>
> ⚠️ **Sicherheit:** Arbeiten an Li-Ion-Akkus auf eigene Gefahr. Die ESP32-Brücke kann nur lesen und die
> 20-Zeichen-Notiz im Akku schreiben. Zähler, Kalibrierung oder Firmware des Akkus fasst sie nicht an.

## Wie wird der Zustand bewertet?

Der Akku speichert keinen echten State-of-Health. Die Software **schätzt** ihn aus den Verschleißzählern im Akku:
100 Punkte minus Abzüge für
- Zyklen (entladene Ah ÷ Nennkapazität)
- Zell-Imbalance
- Ladungen mit einer Zelle < 2,5 V
- Überhitzung
- Kalenderalter

Jeder Abzug wird einzeln angezeigt.
Aussagekräftig ist vor allem der **Verlauf über mehrere Messungen**. Details und Warnschwellen findest du in [gui/README.md](gui/README.md).

## Projektstruktur

```
m18.py                  Protokoll und Kommandozeile (aus m18-protocol)
m18_bridge.py           ESP32-Brücke als M18-Objekt (USB/WLAN), Suche, Abgleich
gui/                    Akku-Verwaltung: Webserver, Datenbank, Alterung, QR, Berichte
  m18_gui.py            Start: python gui/m18_gui.py [--demo]
  fleet.py              SQLite, Zustandsschätzung, Warnungen, Trend
  registers.py          Register → Messwerte (Live und Import identisch)
  bridge_import.py      Messungen der Brücke übernehmen
  qrgen.py              QR-Code-Encoder (reines Python)
  static/               Oberfläche (HTML/CSS/JS, ohne Framework)
webble/                 Bluetooth-Adapter: macht die Weboberfläche der Brücke zur Handy-App (GitHub Pages)
supabase/               Cloud-Datenbank (Schema, Zugriffsregeln, Upload-Funktionen)
esp32-bridge/
  m18_bridge/           Arduino-Sketch + reine C++-Header (Protokoll, Auslesen, Speicher)
  test/                 Simulator und Ende-zu-Ende-Tests
hardware/aufsteckgehaeuse/  3D-Druck-Gehäuse (3MF, STL, Fusion 360), Stückliste, Zusammenbau
docs/                   Bilder, ursprüngliche Protokoll-Doku, Register-Tabelle
```

## Tests

Die Tests laufen ohne Hardware: Ein simulierter Akku spricht das echte Protokoll, und der C++-Kern der Firmware wird
auf dem PC mitgetestet. GitHub Actions führt sie bei jedem Push aus und **kompiliert die Firmware für den ESP32-S3**
mit den Arduino-Cores 2.x und 3.x.

```bash
python -m unittest discover -s gui/tests
```

```bash
g++ -std=c++17 -I esp32-bridge/m18_bridge esp32-bridge/test/sim_bridge.cpp -o sim_bridge
```

```bash
./sim_bridge test
```

```bash
python -m unittest esp32-bridge/test/test_bridge.py esp32-bridge/test/test_history.py
```

Die Brücken-Tests über USB brauchen `pyserial`, alles andere nur die Standardbibliothek.

## Grundlage und Hinweise

- Protokoll, Registertabelle und `m18.py` stammen aus [mnh-jansson/m18-protocol](https://github.com/mnh-jansson/m18-protocol)
  und der Community dort. Die ursprüngliche Anleitung liegt in [docs/m18-protocol.md](docs/m18-protocol.md).
- Kein offizielles Milwaukee-Produkt. „Milwaukee“ und „M18“ sind Marken ihrer jeweiligen Inhaber.
- Änderungen stehen im [CHANGELOG](CHANGELOG.md).
