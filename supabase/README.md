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
