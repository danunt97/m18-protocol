"""
Log file of the PC software: everything the GUI reports (connections, read-outs with the
battery trace of the ESP32 bridge, imports, automatic sync, errors with traceback) goes into
m18_protokoll.log next to the database - in Docker into the /data volume.

    2026-11-07 18:43:12 INFO    Verbunden mit m18-bridge.local – Reset ACK
    2026-11-07 18:43:20 INFO    Brücke/Akku: Wecken 1/3: keine Antwort auf Sync
    2026-11-07 18:43:31 WARNING Auslesen fehlgeschlagen: Akku antwortet nicht (kein Sync)

The file rotates at 2 MB and keeps five old files.
"""

import logging
import logging.handlers
import os

LOG = logging.getLogger("m18")
LOG.setLevel(logging.DEBUG)
LOG.propagate = False
LOG.addHandler(logging.NullHandler())   # until setup(): no file, no output on stderr

MAX_BYTES = 2 * 1024 * 1024
BACKUPS = 5
_path = None


def setup(path):
    """Write the log to `path` (rotating). Safe to call more than once."""
    global _path
    path = os.path.abspath(path)
    if _path == path:
        return path
    for h in list(LOG.handlers):
        LOG.removeHandler(h)
        h.close()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    h = logging.handlers.RotatingFileHandler(path, maxBytes=MAX_BYTES, backupCount=BACKUPS, encoding="utf-8")
    h.setFormatter(logging.Formatter("%(asctime)s %(levelname)-7s %(message)s", "%Y-%m-%d %H:%M:%S"))
    LOG.addHandler(h)
    _path = path
    return path


def path():
    return _path


def read_tail(max_bytes=1024 * 1024):
    """The newest part of the log (current file plus as many rotated files as fit)."""
    if not _path:
        return ""
    for h in LOG.handlers:
        h.flush()
    files = [f"{_path}.{i}" for i in range(BACKUPS, 0, -1)] + [_path]
    parts = []
    for f in files:
        if os.path.isfile(f):
            with open(f, "rb") as fh:
                parts.append(fh.read())
    data = b"".join(parts)
    if len(data) > max_bytes:
        data = data[-max_bytes:]
        data = data[data.find(b"\n") + 1:]
    return data.decode("utf-8", "replace")
