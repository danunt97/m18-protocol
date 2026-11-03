# M18 Akku-Verwaltung als Container (z. B. auf einem NAS).
#   docker compose up -d        (siehe docker-compose.yml)
# Daten (SQLite) liegen im Volume /data.
FROM python:3.13-slim

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    M18_HTTP_PORT=8018
WORKDIR /app

# pyserial nur für einen per USB durchgereichten Adapter / eine USB-Brücke
RUN pip install --no-cache-dir pyserial==3.5

COPY m18.py m18_bridge.py ./
COPY gui ./gui
RUN rm -rf gui/tests

VOLUME ["/data"]
EXPOSE 8018/tcp

HEALTHCHECK --interval=60s --timeout=5s --start-period=20s \
  CMD python -c "import os, urllib.request; urllib.request.urlopen('http://127.0.0.1:%s/api/status' % os.environ.get('M18_HTTP_PORT', '8018'), timeout=4)"

ENTRYPOINT ["python", "gui/m18_gui.py", "--host", "0.0.0.0", "--no-browser", "--db", "/data/m18_fleet.db"]
# Messungen der ESP32-Brücken alle 5 Minuten automatisch übernehmen (in der Oberfläche änderbar)
CMD ["--auto-sync", "5"]
