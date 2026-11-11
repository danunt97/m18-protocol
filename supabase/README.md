# Supabase (Cloud-Rückgrat)

Projekt `m18-akku` (Region Frankfurt, `https://seccxaldsjhukavoilnm.supabase.co`).
Die App und die Brücken speichern hier jede Messung komplett (alle Rohregister = alle 185 Werte, Ablauf, Standort, Kommentar).

`migrations/` enthält das Datenbankschema:

| Tabelle | Inhalt |
|---|---|
| `orgs`, `org_members` | Firma und Mitglieder; jedes neue Konto bekommt automatisch eine eigene Firma (Inhaber) |
| `bridges` | verknüpfte Brücken, Geräteschlüssel nur als SHA-256 gespeichert, sperrbar |
| `batteries` | Akkus pro Firma (E-Seriennummer eindeutig) |
| `readings` | Messungen inkl. `mem` (Rohregister) und `scan` (komplettes Ergebnis der Brücke); doppelte Uploads werden erkannt |

Funktionen:

- `app_ingest(org, payload, source)`: Upload aus der App (angemeldet)
- `bridge_register(org, name, mac)`: Brücke verknüpfen, liefert den Geräteschlüssel einmalig
- `bridge_ingest(token, payload, firmware)`: Upload der Brücke mit ihrem Geräteschlüssel

`payload` = `{"scan": <Ergebnis von SCAN>, "meta": {"ts": <unix>, "ts_src": "...", "loc": "...", "comment": "..."}}`.

Zugriff nur über Row Level Security: Mitglieder sehen die Daten ihrer Firma, sonst niemand.
Der öffentliche Schlüssel (publishable/anon) darf in der App stehen; der service_role-Schlüssel nie.

## Einstellungen (gesetzt über die Supabase-Verwaltung)

- Anmeldung per E-Mail und Passwort (mindestens 8 Zeichen), Bestätigungs-Mail an.
- Site URL `https://akkuscan.app/`, erlaubte Weiterleitungen `https://akkuscan.app/**` und (alt) `https://danunt97.github.io/m18-protocol/**`.
- E-Mail-Texte: Im Gratis-Tarif mit dem eingebauten Mailversand sind nur die englischen Standardtexte möglich und nur
  wenige Mails pro Stunde. Für deutsche Texte und mehr Mails einen eigenen SMTP-Versand eintragen (Authentication → Emails → SMTP).

## Geprüft

Mit zwei Testnutzern direkt in der Datenbank: eigene Firma bei der Registrierung, App-Upload, doppelte Messung (App und Brücke)
wird nur einmal gespeichert, Brücke verknüpfen und mit Geräteschlüssel hochladen, falscher Schlüssel abgelehnt,
Schlüssel-Hash nicht lesbar, fremde Nutzer sehen und ändern nichts. Danach wieder entfernt.

## Admin-Seite (akkuscan.app/admin/)

Übersicht (Kennzahlen, Verlauf pro Tag), alle Konten, Messungen (mit Rohdaten), Akkus, Module und Seitenbesuche,
dazu „Konto löschen“ und „Modul sperren“. Die Daten kommen nur über die Funktionen `admin_*`
(Migration `20261111090000_admin_and_page_views.sql`); jede prüft zuerst `is_admin()`.

Admin eintragen (Supabase → SQL Editor):

```sql
insert into public.admins (user_id) select id from auth.users where email = 'deine@adresse';
```

Seitenbesuche zählt `webland/track.js` über `track_view()`: pro Aufruf Pfad, Herkunfts-Domain, Gerätetyp, Browser,
Sprache und `utm_source` – ohne Cookies, IP-Adresse oder Besucher-ID. Gezählt wird nur auf akkuscan.app.
