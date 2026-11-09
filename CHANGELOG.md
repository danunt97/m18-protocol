# Changelog

## Eigene Domain: akkuscan.app
- Das Portal (Akku-App mit Bluetooth, Konto, Meine Akkus) liegt unter **https://akkuscan.app/**, der Firmware-Flasher unter **https://akkuscan.app/flasher/**.
  Alte Links auf `/app/` leiten weiter, github.io-Adressen leitet GitHub auf die Domain um.
- Supabase: Bestätigungs- und Passwort-Mails führen auf akkuscan.app.

## 1.12 – Eine App, Cloud als Rückgrat
- **Eine App für WLAN und Bluetooth:** Die Akku-App auf GitHub Pages ist jetzt die Weboberfläche der Brücke selbst, mit einem Bluetooth-Adapter (`webble/ble-shim.js`).
  Über Bluetooth geht damit alles wie im WLAN: Auslesen, Verlauf, WLAN einrichten, LED, Energiesparen, Passwort, Protokoll, Cloud.
  Die Firmware beantwortet alle `/api/...`-Funktionen über HTTP und über den Befehl `API …` mit derselben Umsetzung.
- **Cloud (Supabase):** Konto mit E-Mail und Passwort, eigene Firma, „Meine Akkus“ mit Verlauf und jeder Messung komplett (alle 185 Werte).
  Messungen werden nach dem Auslesen hochgeladen. Ohne Internet warten sie und werden nachgeholt. Konto löschen ist möglich.
- **Brücke lädt selbst hoch:** Nach dem Verknüpfen in der App überträgt die Brücke ihre gespeicherten Messungen per HTTPS, sobald sie im WLAN Internet hat.
  Der Messungsspeicher kennt dafür „in der Cloud“ neben „am PC übernommen“. Doppelte Uploads speichert die Datenbank nur einmal.
- Datenbank: Schema, Zugriffsregeln und Upload-Funktionen in `supabase/migrations/`, getestet mit Testkonten gegen das echte Projekt.

## 1.11 – Bluetooth und Handy-App
- **Bluetooth LE** in der Firmware (Nordic-UART-Dienst) mit denselben Textbefehlen wie USB und WLAN, dazu `AUTH`, `META` und `SCAN` mit Fortschritt.
  WLAN-Hotspot und Weboberfläche bleiben, Verbindungen und Auslesen per Bluetooth stehen im Protokoll.
- **Akku-App im Browser** (`webble/`, auf GitHub Pages unter `/app/`): Android in Chrome, iPhone in der kostenlosen App „Bluefy“.
  Auslesen mit Standort und Kommentar, alle Werte, Verlauf und Protokoll der Brücke, Passwortabfrage. Kein App Store und kein Entwicklerkonto nötig.
- Die Anzeige (Stil und Ergebnisdarstellung) ist ein gemeinsamer Block der Weboberfläche. Die App wird daraus erzeugt (`tools/gen_app.py`), dadurch sehen beide gleich aus.
- Web-Flasher-Seite verweist auf die App.

## 1.10 – Protokolle für Akku und System
- **Brücke:** Ereignisprotokoll im Flash. Es enthält Start mit Neustartgrund, Einstellungen, WLAN-Ereignisse, Befehle vom PC und von der Webseite (mit Absender),
  falsche Passwörter, Updates, Neustarts, Tiefschlaf und alle 30 min eine Statuszeile. Es übersteht Neustarts.
  Zeilen, die vor einem Absturz, Watchdog oder Brownout noch nicht im Flash waren, kommen aus dem RTC-Speicher zurück.
  Anzeige und Download unter Gerät → Protokoll, `LOG [n]`, `/api/log`, `/api/log.txt`.
- **Akku-Ablauf:** Jedes Auslesen protokolliert Weckversuche (mit der Antwort des Akkus), abgelehnte und fehlende Register,
  Wiederholungen und das Ergebnis. Steht im Protokoll, im Ergebnis (`trace`) und auf der Webseite („Ablauf beim Auslesen“).
- **PC-Software:** Protokolldatei `m18_protokoll.log` neben der Datenbank (rotierend). Der Akku-Ablauf der Brücke wird übernommen,
  Fehler stehen mit vollständiger Meldung drin. Neu: PC-Protokoll herunterladen, Brücken-Protokoll anzeigen, Diagnose-Datei.

## 1.9 – Auslesen vom PC über die Brücke robust und schnell
- Die PC-Software liest über die ESP32-Brücke jetzt mit **einem** Befehl (`SCAN NOSTORE`) statt mit rund 220 einzelnen Registerabfragen.
  Damit gelten auch beim Auslesen vom PC alle Härtungen der Brücke: mehrere Weckversuche, automatischer zweiter Durchlauf, Wiederholungen pro Block, nicht unterstützte Register (ältere 5-Ah-Akkus) werden übersprungen.
  Bisher wurde der Akku vom PC aus nur einmal geweckt; verpasste er das, lief jede einzelne Abfrage in eine Zeitüberschreitung.
- Das Ergebnis wird mit demselben Code dekodiert wie ein Auslesen über einen USB-Adapter. Die Messung landet nicht zusätzlich im Speicher der Brücke (`NOSTORE`), gespeichert wird wie gewohnt in der PC-Datenbank.
- `RESET` der Brücke versucht das Wecken bis zu dreimal.
- Ältere Firmware ohne Rohdaten im Ergebnis: Die PC-Software liest wie bisher Register für Register.

## 1.8 – Alle Werte auch auf dem Handy
- Die Weboberfläche der Brücke zeigt nach dem Auslesen **alle 185 Werte**, die der Akku liefert (dieselben Register wie die PC-Software): Grunddaten, aktuelle Messwerte, Zähler, Entladezeit nach Strom (10-A- und 5-A-Stufen), Ladungen nach Spannung und Temperatur, Ladedauer-Histogramme, unbekannte Register. Histogramme als Balken, deutsche Bezeichnungen, Registeradresse dazu.
- Werte, die ein Akku nicht liefert, stehen als „nicht verfügbar“.
- Export direkt vom Handy: „Alle Werte als CSV“ (Excel, mit Rohwerten) und „Rohdaten (JSON)“; auch für gespeicherte Messungen im Tab „Verlauf“.
- Die Tabelle wird aus `m18.py` erzeugt (`esp32-bridge/tools/gen_regs.py`); die Tests prüfen, dass sie aktuell ist.

## 1.7 – Ältere Akkus (5 Ah XC) direkt auf der Brücke
- Ältere Akkus kennen die 13 Forge-Registerblöcke (0x4000–0x401F, 0x6000–0x600C) nicht und lehnen sie ab bzw. antworten nicht. Die Brücke hat das bisher nach drei Blöcken als „Verbindung zum Akku abgebrochen“ gewertet. Jetzt gilt: Antwortet der Akku noch auf den Sync, ist der Block nur „nicht unterstützt“ und das Auslesen läuft weiter (wie bei m18.py, das „------“ zeigt).
- Nicht unterstützte Blöcke werden im zweiten Durchlauf nicht noch einmal mit Wiederholungen abgefragt; das spart Zeit. Neu im Ergebnis: `blocks_unsupported`.
- Liefert ein Akku keine Zellspannungen, erscheint ein Hinweis statt „Zellspannungen unplausibel“.
- PC: Zeitlimit für `SCAN` über die Brücke auf 90 s erhöht.
- Schlägt das Auslesen fehl, versucht es die Brücke nach 1,5 s Pause (J2 low) automatisch ein zweites Mal. Der Grund des ersten Fehlversuchs steht im Ergebnis (`attempts`, `first_error`) und auf der Webseite.

## 1.6 – Stabilität auf echter Hardware
- Watchdog-Neustart behoben: Ein Handy, das mitten in einer Anfrage hängen blieb, konnte den Webserver länger als die 5 s des Watchdogs blockieren. Der Watchdog der Hauptschleife steht jetzt auf 30 s.
- Unterspannung beim WLAN-Start: Die Brücke zählt Brownout-Neustarts, fährt danach das WLAN mit reduzierter Sendeleistung hoch und zeigt eine Warnung unter Gerät → „Stromversorgung“. Normale Sendeleistung jetzt 15 dBm (geringere Stromspitzen).
- Web-Flasher: Das Image ist nur noch ~1,3 MB statt 16 MB groß (leere Bytes am Ende abgeschnitten). Das Flashen dauert Sekunden statt Minuten und hängt nicht mehr lange bei 98 %.
- Statusmeldungen („Setup-WLAN …“, „M18BRIDGE READY …“) erscheinen auf beiden USB-Buchsen.

## 1.5 – Sicherung, LED-Ergebnis, Web-Flasher, Docker
- Aufsteckgehäuse: Sicherung T 500 mA im B+-Pfad (Stückliste, Verdrahtungsplan, Anleitung).
- LED zeigt nach dem Auslesen das Ergebnis 30 s lang (grün/gelb/rot), auch bei Messungen vom PC (`SHOW`).
- Firmware im Browser installieren (ESP Web Tools auf GitHub Pages); Firmware-Dateien hängen an jedem Release.
- Docker-Image (amd64/arm64, ghcr.io) und `docker-compose.yml` für den NAS.
- Automatischer Abgleich: Die Akku-Verwaltung übernimmt neue Messungen der Brücken selbstständig. Diese Abfragen halten die Brücke nicht wach.

## 1.4 – Aufsteckgehäuse und Energiesparen
- 3D-druckbares Aufsteckgehäuse (Bambu-3MF, STL, Fusion 360) mit Stückliste und Zusammenbau in `hardware/aufsteckgehaeuse/`.
- Versorgung aus dem Akku über einen Step-down-Wandler (HW-411/LM2596); der Verdrahtungsplan ist entsprechend ergänzt.
- Energiesparmodus der Brücke: Tiefschlaf nach einstellbarer Ruhezeit. J2 wird dabei per Pin-Hold auf Low gehalten, die BOOT-Taste weckt.

## 1.3 – Offline-Messungen und Passwort
- ESP32-Brücke speichert jede Messung im Flash: Zeit, Standort/Kunde, Kommentar und alle Rohregister.
- Neuer Tab „Verlauf“ auf der Brücke: Liste, Ansicht, Löschen, JSON-Export.
- PC-Software übernimmt die Messungen in die Datenbank. Doppelte werden erkannt und übersprungen, die Zeit stammt von NTP, Handy, PC oder der Laufzeit der Brücke.
- Import einer JSON-Exportdatei der Brücke in den Einstellungen.
- Optionaler Passwortschutz (Weboberfläche und PC-Zugriff). BOOT 5 s setzt ihn zurück.

## 1.2 – Status-LED und Härtung
- Onboard-RGB-LED: Pin und Helligkeit sind in der Weboberfläche einstellbar, mit Test-Knopf.
- Lesen mit Wiederholungen, Resync und schnellem Abbruch bei gezogenem Akku; Plausibilitätsprüfung.
- Auslesen im Hintergrund-Task, Mutex für die Akku-Leitung, Watchdog, Heap-Wächter.
- Einstellungen nur noch über das Einrichtungs-WLAN, USB oder nach einem BOOT-Tastendruck änderbar.
- Firmware-Update im Browser.
- J2-Ruhezustand über TX-Invertierung. USB ist sofort nach dem Start bereit.

## 1.1 – ESP32-S3-Brücke
- WLAN-Einrichtung mit Captive Portal, automatische Suche (UDP und mDNS).
- Direktes Auslesen auf der Brücke mit eigener Weboberfläche.
- Die GUI erkennt ESP32-S3 am USB und findet Brücken im WLAN.

## 1.0 – Akku-Verwaltung
- Webbasierte Akku-Verwaltung mit SQLite-Datenbank, Flottenübersicht und Alterungsdiagrammen.
- Warnungen, Standort und Kommentar je Akku.
- QR-Etiketten (P-touch/Brother QL), Berichte, CSV-Export, Demo-Modus.
