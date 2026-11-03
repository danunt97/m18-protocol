# Changelog

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
