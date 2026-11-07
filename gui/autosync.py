"""
Automatic background sync: every few minutes, find the ESP32 bridges on the
network (UDP discovery + hosts entered in the settings) and import their new
measurements into the database - no "Übernehmen" click needed. Meant for the
always-on installation (e.g. Docker on a NAS).

Requests carry the "X-M18-Background" header, so they do not reset the
bridge's energy-saving timer: a battery-powered bridge still falls asleep.
"""

import datetime as _dt
import logging
import threading
import time

import m18_bridge
from bridge_import import import_from_bridge
from fleet import iso, utcnow


class AutoSync:
    def __init__(self, db, log, default_interval_min=0):
        self.db = db
        self.log = log
        self.default_interval = default_interval_min
        self.status = {"running": False, "last_run": None, "last_results": [], "next_run": None}
        self._lock = threading.Lock()
        self._wake = threading.Event()
        self._stop = False
        self._thread = None

    # settings (stored in the database, CLI value as default) ---------------
    def interval_min(self):
        v = self.db.get_setting("autosync_min")
        return int(v) if v not in (None, "") else int(self.default_interval)

    def hosts(self):
        return [h.strip() for h in (self.db.get_setting("bridge_hosts") or "").replace(";", ",").split(",")
                if h.strip()]

    def configure(self, interval_min=None, hosts=None):
        if interval_min is not None:
            self.db.set_setting("autosync_min", str(max(0, min(1440, int(interval_min)))))
        if hosts is not None:
            self.db.set_setting("bridge_hosts", str(hosts).strip())
        self._wake.set()
        return self.info()

    def info(self):
        return {**self.status, "interval_min": self.interval_min(), "hosts": self.db.get_setting("bridge_hosts") or ""}

    # one pass -------------------------------------------------------------
    def run_once(self, discover_timeout=1.5):
        if not self._lock.acquire(blocking=False):
            return {"ok": False, "error": "Abgleich läuft bereits"}
        try:
            self.status["running"] = True
            targets = []
            try:
                for b in m18_bridge.discover(timeout=discover_timeout):
                    targets.append(b.get("ip"))
            except OSError as e:
                self.log(f"Abgleich: Suche fehlgeschlagen ({e})", logging.WARNING)
            for h in self.hosts():
                if h not in targets:
                    targets.append(h)
            results = []
            password = self.db.get_setting("bridge_password") or None
            for t in targets:
                entry = {"target": t, "ok": False}
                try:
                    bridge = m18_bridge.M18Bridge(t, password=password, background=True)
                    try:
                        stats = bridge.history_stats()
                        entry.update(ok=True, unsynced=stats.get("unsynced", 0), imported=0, created=0, duplicates=0)
                        if stats.get("unsynced"):
                            res = import_from_bridge(self.db, bridge)
                            entry.update(imported=res["imported"], created=res["created"],
                                         duplicates=res["duplicates"], errors=res["errors"])
                            if res["imported"] or res["created"]:
                                self.log(f"Automatischer Abgleich {t}: {res['imported']} Messungen übernommen, "
                                         f"{res['created']} neue Akkus")
                    finally:
                        bridge.port.close()
                except m18_bridge.BridgeAuthError:
                    entry["error"] = "Passwort fehlt oder falsch (Einstellungen → Brücken-Passwort)"
                except Exception as e:  # unreachable, asleep, old firmware ...
                    entry["error"] = str(e)
                if entry.get("error"):
                    self.log(f"Automatischer Abgleich {t}: {entry['error']}", logging.WARNING)
                results.append(entry)
            self.status.update(last_run=iso(utcnow()), last_results=results)
            self.log(f"Automatischer Abgleich: {len(targets)} Brücke(n) geprüft"
                     + (": " + ", ".join(f"{e['target']} {'ok' if e['ok'] else 'Fehler'}" for e in results) if results else ""),
                     logging.DEBUG)
            return {"ok": True, "results": results}
        finally:
            self.status["running"] = False
            self._lock.release()

    # background thread ------------------------------------------------------
    def start(self):
        if self._thread:
            return
        self._thread = threading.Thread(target=self._loop, name="autosync", daemon=True)
        self._thread.start()

    def stop(self):
        self._stop = True
        self._wake.set()

    def _loop(self):
        time.sleep(5)                       # let the web server come up first
        while not self._stop:
            minutes = self.interval_min()
            if minutes > 0:
                try:
                    self.run_once()
                except Exception as e:      # never let the thread die
                    self.log(f"Automatischer Abgleich: Fehler {e}")
                wait = minutes * 60
            else:
                wait = 30                   # disabled: check the setting again later
            self.status["next_run"] = iso(utcnow() + _dt.timedelta(seconds=wait)) if minutes > 0 else None
            self._wake.wait(wait)
            self._wake.clear()
