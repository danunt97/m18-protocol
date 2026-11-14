/* Demo mode of the Akkuscan app: akkuscan.app/app/?demo
 * Everything runs in the browser – a simulated module (over a simulated Bluetooth connection) and a simulated cloud
 * with an example fleet. Nothing is sent to the real cloud and the visitor's own login and data stay untouched
 * (the demo keeps its own copy of the browser storage under "demo.*").
 * esp32-bridge/tools/gen_app.py puts this in front of ble-shim.js. Without ?demo it does nothing. */
(function () {
  "use strict";
  if (!/[?&]demo\b/.test(location.search)) return;
  window.M18_DEMO = true;

  // ------------------------------------------------------------ separate storage (keeps the real login untouched)
  try {
    const real = window.localStorage, P = "demo.", g = Storage.prototype.getItem, s = Storage.prototype.setItem, r = Storage.prototype.removeItem;
    Storage.prototype.getItem = function (k) { return g.call(this, this === real ? P + k : k); };
    Storage.prototype.setItem = function (k, v) { return s.call(this, this === real ? P + k : k, v); };
    Storage.prototype.removeItem = function (k) { return r.call(this, this === real ? P + k : k); };
    if (!real.getItem("m18.sb.sess")) real.setItem("m18.sb.sess", JSON.stringify({ at: "demo", rt: "demo", exp: 4e9, email: "demo@akkuscan.app" }));
    if (!real.getItem("m18.sb.org")) real.setItem("m18.sb.org", JSON.stringify({ id: "demo-org", name: "Demo Elektro GmbH" }));
    real.removeItem("m18.sb.queue");
  } catch (e) {}

  // ------------------------------------------------------------ example batteries
  const DAY = 86400, now = () => Math.floor(Date.now() / 1000), iso = t => new Date(t * 1000).toISOString();
  const r1 = v => Math.round(v * 10) / 10;
  // key, e-serial, type code, model, Ah, manufactured, cells (mV), temp, charges, full cycles, overheat, low-voltage events, low-volt charges, name, group, location
  const B = [
    ["b5", "424-6630019", 424, "5Ah XC (>= 2023)", 5, "2024-03-02", [4071, 4069, 4074, 4070, 4072], 18.9, 132, 96, 0, 3, 0, "Akkuschrauber 1", "g2", "Werkstatt"],
    ["b2", "107-2210371", 107, "8Ah HO", 8, "2021-09-14", [3988, 3951, 3990, 3985, 3992], 21.4, 470, 352, 4, 21, 2, "Bus 1 – Akku 2", "g1", "Firmenbus 1"],
    ["b4", "384-1022871", 384, "12Ah Forge", 12, "2025-02-20", [4102, 4101, 4103, 4102, 4100], 17.2, 38, 24, 0, 0, 0, "Werkstatt Forge", "g2", "Werkstatt"],
    ["b6", "39-0923743", 39, "4Ah XC", 4, "2016-08-08", [4012, 3893, 4015, 4010, 3990], 16.1, 610, 410, 9, 112, 7, "Alter Akku", "g2", "Werkstatt"],
    ["b8", "383-1190044", 383, "8Ah Forge", 8, "2025-08-30", [4141, 4140, 4142, 4141, 4139], 15.8, 9, 5, 0, 0, 0, null, null, null],
    ["b1", "107-2210345", 107, "8Ah HO", 8, "2023-05-11", [4031, 4029, 4033, 4030, 4032], 19.5, 140, 86, 0, 2, 0, "Bus 1 – Akku 1", "g1", "Firmenbus 1"],
    ["b3", "108-1873320", 108, "12Ah HO", 12, "2022-11-03", [4056, 4049, 4058, 4051, 4055], 20.1, 210, 150, 1, 6, 0, "Bus 1 – Bohrhammer", "g1", "Firmenbus 1"],
    ["b7", "37-4410287", 37, "2Ah CP", 2, "2023-01-19", [4096, 4090, 4097, 4094, 4095], 18.0, 160, 118, 0, 9, 1, "Büro Reserve", "g3", "Büro"],
  ].map(a => ({ key: a[0], eserial: a[1], type: a[2], model: a[3], cap: a[4], man: a[5], cells: a[6], temp: a[7], charges: a[8], cycles: a[9],
    overheat: a[10], lowvolt: a[11], lowv: a[12], name: a[13], group: a[14], loc: a[15] }));

  // register template of a real read-out (sim of the firmware); a few registers are set per battery so "Alle Werte" matches
  const MEM = {"0000":"0005","0002":"0000","0004":"0132064CCD","000D":"00000000","0011":"622E8580","0015":"00000000","0019":"00000000","0023":"2D2D2D2D2D2D2D2D2D2D2D2D2D2D2D2D2D2D2D2D","0037":"69054D80","0069":"0000","007B":"00","4000":"00000000","400A":"0FAC0FAA0FAF0FA90FAB","4014":"02B9","4016":"0000","4019":"0000","401B":"0000","401D":"0000","401F":"0000","6000":"0000","6002":"0000","6004":"00000000","6008":"00000000","600C":"0000","9000":"0000000000000000000000000000000006860036EE80000000000000010400000000000000000000000000000000000000000002000000060000","903A":"0E100708000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","9074":"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","90AE":"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","90E8":"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","9122":"000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","A000":"000000000000"};
  function setReg(mem, addr, len, value) {
    for (const k in mem) {
      const start = parseInt(k, 16), n = mem[k].length / 2;
      if (addr >= start && addr + len <= start + n) {
        let v = Math.floor(value); const bytes = [];
        for (let i = 0; i < len; i++) { bytes.unshift(v % 256); v = Math.floor(v / 256); }
        const hex = bytes.map(x => x.toString(16).padStart(2, "0")).join("").toUpperCase();
        const o = (addr - start) * 2; mem[k] = mem[k].slice(0, o) + hex + mem[k].slice(o + len * 2); return;
      }
    }
  }
  function scanOf(b, t) {
    const man = Date.parse(b.man + "T00:00:00Z") / 1000, years = (t - man) / 31557600;
    const imb = Math.max(...b.cells) - Math.min(...b.cells), dis = r1(b.cycles * b.cap);
    const parts = [
      { label: "Zyklenverschleiß", penalty: r1(b.cycles * 0.04), detail: Math.round(b.cycles) + " äquivalente Vollzyklen" },
      { label: "Zell-Imbalance", penalty: r1(Math.max(0, imb - 15) * 0.35), detail: imb + " mV Spreizung" },
      { label: "Überhitzung", penalty: r1(b.overheat * 1.5), detail: b.overheat + " Ereignisse" },
      { label: "Kalenderalter", penalty: r1(years * 2), detail: years.toFixed(1) + " Jahre" }];
    if (b.lowv) parts.splice(3, 0, { label: "Tiefentladung", penalty: r1(b.lowv * 2), detail: b.lowv + " Ladungen mit Zelle < 2,5 V" });
    const health = Math.max(0, Math.min(100, Math.round(100 - parts.reduce((a, p) => a + p.penalty, 0))));
    const status = health >= 75 ? "good" : health >= 50 ? "watch" : health >= 35 ? "weak" : "crit";
    const c = { total_charges: b.charges, redlink_charges: Math.round(b.charges * .7), dumb_charges: b.charges - Math.round(b.charges * .7),
      discharge_as: Math.round(dis * 3600), discharge_ws: Math.round(dis * 3600 * 18.4), overheat: b.overheat, overcurrent: 0, lowvolt: b.lowvolt,
      lowvolt_bounce: 0, discharge_to_empty: Math.round(b.lowvolt / 3), lowv_charges: b.lowv, days: Math.round((t - man) / DAY),
      charge_time_s: b.charges * 2350, tool_time_s: Math.round(dis * 60 * 2.2) };
    const mem = JSON.parse(JSON.stringify(MEM)), ser = +b.eserial.split("-")[1];
    setReg(mem, 0x0000, 2, b.type == 383 || b.type == 384 ? 7 : b.type >= 104 && b.type <= 150 ? 6 : 5);
    setReg(mem, 0x0004, 2, b.type); setReg(mem, 0x0006, 3, ser % 16777216); setReg(mem, 0x0011, 4, man); setReg(mem, 0x0037, 4, t - t % DAY);
    b.cells.forEach((v, i) => setReg(mem, 0x400A + i * 2, 2, v));
    [[0x9010, 2, c.days], [0x9012, 4, c.discharge_as], [0x9016, 4, c.discharge_ws], [0x901A, 4, c.total_charges], [0x901E, 2, c.dumb_charges],
      [0x9020, 2, c.redlink_charges], [0x9024, 4, c.charge_time_s], [0x902E, 2, c.lowv_charges], [0x9030, 2, c.discharge_to_empty],
      [0x9032, 2, c.overheat], [0x9034, 2, c.overcurrent], [0x9036, 2, c.lowvolt], [0x9038, 2, c.lowvolt_bounce]].forEach(x => setReg(mem, x[0], x[1], x[2]));
    return { ok: true, blocks_ok: 32, blocks_total: 32, blocks_unsupported: 0, duration_ms: 6400 + Math.round(Math.random() * 900), retries: 0,
      checksum_mismatch: 0, attempts: 1, trace: ["Auslesen gestartet", "1. Durchlauf: 32 ok", "Fertig: Typ " + b.type + " Nr. " + ser + ", 32/32 Blöcke (Demo)"],
      eserial: b.eserial, type_code: b.type, model: b.model, capacity_ah: b.cap, manufactured: b.man, first_charge: null, last_charge: null,
      last_tool_use: null, battery_date: iso(t).slice(0, 10), note: "", warnings: imb > 80 ? ["Große Spreizung zwischen den Zellen (" + imb + " mV)"] : [],
      cells_mv: b.cells.slice(), pack_v: r1(b.cells.reduce((a, v) => a + v, 0) / 100) / 10, imbalance_mv: imb, temp_c: b.temp, counters: c,
      discharge_ah: dis, discharge_wh: Math.round(dis * 18.4), health_parts: parts, cycles: r1(b.cycles), health, status, mem };
  }

  // ------------------------------------------------------------ simulated module (answers the page's /api calls)
  const INFO = { version: "1.13", line: "idle", wifi: "ap", ip: "", host: "akkuscan-demo.local", ssid: "", ap: "Akkuscan-Demo", ap_ip: "192.168.4.1",
    rssi: 0, mac: "02:00:00:DE:40:01", uptime_s: 0, heap_kb: 180, psram_kb: 8000, min_heap_kb: 170, reset_reason: "Einschalten", build: "demo",
    led_pin: 48, led_brightness: 40, unlocked: true, unlock_s: 120, setup_client: false, reading: false, auth: false, storage: "FAT",
    ble: "Akkuscan-Demo", ble_connected: true, cloud: false, sleep_min: 30 };
  const t0 = now();
  let next = 0, read = null, last = null, hid = 0;
  const HIST = [];
  const meta = (s, loc, comment, ts) => ({ synced: false, cloud: false, id: ++hid, ts, ts_src: "app", boot: 1, up: 0, eserial: s.eserial,
    model: s.model, health: s.health, note: "", loc: loc || "", comment: comment || "" });
  [[B[1], 3], [B[6], 2], [B[7], 1]].forEach(([b, d]) => { const ts = now() - d * DAY, s = scanOf(b, ts); HIST.push({ meta: meta(s, b.loc, "", ts), scan: s }); });
  const qs = q => Object.fromEntries(new URLSearchParams(q || ""));
  const stats = () => ({ count: HIST.length, unsynced: HIST.filter(h => !h.meta.synced).length, cloud_pending: 0, free_kb: 30000, total_kb: 31000, ready: true });
  const CLOUD = { configured: false };
  function api(method, path, body) {
    const a = qs(body);
    const finish = () => { if (read && Date.now() - read.at >= 3200 && !read.done) {
      read.done = true; const ts = +a.t || now(), s = scanOf(read.b, ts); const m = meta(s, read.loc, read.comment, ts);
      HIST.push({ meta: m, scan: s }); last = Object.assign({}, s, { history_id: m.id }); } };
    switch (path.split("?")[0]) {
      case "/api/info": finish(); return [200, Object.assign({}, INFO, { uptime_s: now() - t0, reading: !!(read && !read.done), cloud: CLOUD.configured })];
      case "/api/read": read = { at: Date.now(), b: B[next++ % B.length], loc: a.loc, comment: a.comment, done: false }; return [200, { ok: true, started: true }];
      case "/api/status": { finish(); const p = read ? Math.min(100, Math.round((Date.now() - read.at) / 32)) : 100;
        return [200, { reading: !!(read && !read.done), progress: p, done_ms_ago: 0 }]; }
      case "/api/last": finish(); return [200, last || {}];
      case "/api/history": { const q = qs(path.split("?")[1]); let items = HIST.map(h => h.meta).slice().reverse();
        if (q.filter == "new") items = items.filter(m => !m.synced);
        return [200, Object.assign({ items, next: 0 }, stats())]; }
      case "/api/history/stats": return [200, stats()];
      case "/api/history/item": { const id = +qs(path.split("?")[1]).id, h = HIST.find(x => x.meta.id == id); return h ? [200, h] : [404, { ok: false, error: "nicht gefunden" }]; }
      case "/api/history/delete": case "/api/history/clear": HIST.splice(0, HIST.length); return [200, { ok: true }];
      case "/api/log": return [200, { boot: 1, total: 3, lines: ["B1 +0.0s SYS Demo-Modul gestartet, Firmware 1.13", "B1 +0.2s BLE verbunden", "B1 +0.4s SYS Demo: Beispieldaten, kein echter Akku"] }];
      case "/api/wifi":
        if (method == "POST") return [400, { ok: false, error: "In der Demo nicht möglich" }];
        return [200, { state: "ap", ssid: "", ip: "", host: INFO.host, rssi: 0, ap: INFO.ap, ap_closes_in: 0, error: "", allowed: true }];
      case "/api/networks": return [200, { scanning: false, networks: [{ ssid: "Werkstatt-WLAN", rssi: -52, secure: true }, { ssid: "Baustelle-Gast", rssi: -71, secure: false }] }];
      case "/api/cloud":
        if (method == "POST") { if (a.action == "link") CLOUD.configured = true; if (a.action == "off") CLOUD.configured = false; return [200, { ok: true }]; }
        return [200, { configured: CLOUD.configured, host: "akkuscan.app", pending: 0, uploaded: HIST.length, last_ok: CLOUD.configured ? now() : 0, online: true, busy: false, error: "" }];
      default: return [200, { ok: true }];
    }
  }
  // simulated Bluetooth device "Akkuscan-Demo" speaking the module's line protocol
  const te = new TextEncoder(), listeners = [];
  let buf = "", connected = false, onDisc = null;
  const notify = line => { const bytes = te.encode(line + "\n"); for (let o = 0; o < bytes.length; o += 180) { const part = bytes.slice(o, o + 180);
    listeners.forEach(l => l({ target: { value: new DataView(part.buffer, part.byteOffset, part.byteLength) } })); } };
  function handle(line) {
    const m = line.match(/^API (GET|POST) (\S+)(?: (.*))?$/);
    if (m) { const [st, obj] = api(m[1], m[2], m[3] || ""); return (st == 200 ? "OK " : "ERR " + st + " ") + JSON.stringify(obj); }
    if (/^LOG/.test(line)) return "OK " + JSON.stringify(api("GET", "/api/log", "")[1]);
    return "OK {}";
  }
  const rx = { async writeValueWithResponse(part) { buf += new TextDecoder().decode(part); let i;
    while ((i = buf.indexOf("\n")) >= 0) { const line = buf.slice(0, i); buf = buf.slice(i + 1); const reply = handle(line);
      setTimeout(() => { if (connected) notify(reply); }, 25 + Math.random() * 40); } } };
  const tx = { addEventListener(_, f) { listeners.push(f); }, async startNotifications() {} };
  const svc = { async getCharacteristic(u) { return u.startsWith("6e400002") ? rx : tx; } };
  const device = { id: "akkuscan-demo", name: "Akkuscan-Demo", addEventListener(e, f) { if (e === "gattserverdisconnected") onDisc = f; },
    gatt: { get connected() { return connected; }, async connect() { await new Promise(s => setTimeout(s, 400)); connected = true; return { async getPrimaryService() { return svc; } }; },
      disconnect() { const was = connected; connected = false; if (was && onDisc) onDisc(); } } };
  try { Object.defineProperty(navigator, "bluetooth", { configurable: true, value: {
    async requestDevice() { await new Promise(s => setTimeout(s, 300)); return device; },
    async getAvailability() { return true; }, async getDevices() { return []; } } }); } catch (e) {}

  // ------------------------------------------------------------ simulated cloud (account, fleet, groups, read-outs)
  const SBURL = "https://seccxaldsjhukavoilnm.supabase.co", ORG = "demo-org";
  let seq = 1; const uid = p => p + "-" + (seq++);
  const DB = { groups: [
      { id: "g1", org_id: ORG, name: "Firmenbus 1", color: "#e1251b", icon: "van", sort: 0 },
      { id: "g2", org_id: ORG, name: "Werkstatt", color: "#ff7a00", icon: "workshop", sort: 1 },
      { id: "g3", org_id: ORG, name: "Büro", color: "#f2f2f2", icon: "office", sort: 2 }],
    bats: [], readings: [] };
  B.forEach((b, i) => {
    const id = "bat-" + b.key; DB.bats.push({ id, org_id: ORG, eserial: b.eserial, type_code: b.type, model: b.model, capacity_ah: b.cap, name: b.name,
      location: b.loc, comment: null, retired: false, group_id: b.group, created_at: iso(now() - 200 * DAY) });
    const n = b.key == "b8" ? 1 : 3 + (i % 4);
    for (let k = n - 1; k >= 0; k--) {
      const ts = now() - (k * 32 + 1 + i) * DAY, older = Object.assign({}, b, { cycles: Math.max(0, b.cycles - k * 9), charges: Math.max(1, b.charges - k * 12) });
      const s = scanOf(older, ts);
      DB.readings.push({ id: uid("r"), org_id: ORG, battery_id: id, measured_at: iso(ts), health: s.health, status: s.status, source: k % 2 ? "bridge" : "app-ble",
        location: b.loc, comment: k == 0 && b.key == "b6" ? "Hält nur noch kurz – austauschen" : null, pack_v: s.pack_v, scan: s });
    }
  });
  const overview = () => DB.bats.map(b => { const rs = DB.readings.filter(r => r.battery_id == b.id).sort((x, y) => y.measured_at.localeCompare(x.measured_at)), l = rs[0] || {};
    return Object.assign({}, b, { last_measured_at: l.measured_at || null, last_health: l.health != null ? l.health : null, last_status: l.status || null,
      last_pack_v: l.pack_v || null, last_imbalance_mv: l.scan ? l.scan.imbalance_mv : null, last_cycles: l.scan ? l.scan.cycles : null, last_source: l.source || null, readings: rs.length }); });
  const eq = (u, k) => { const v = u.searchParams.get(k); return v && v.startsWith("eq.") ? decodeURIComponent(v.slice(3)) : null; };
  function cloud(method, u, body) {
    const p = u.pathname, b = body ? JSON.parse(body) : {};
    if (p.startsWith("/auth/v1/")) {
      if (/token|signup/.test(p)) return [200, { access_token: "demo", refresh_token: "demo", expires_in: 3600000, user: { email: "demo@akkuscan.app" } }];
      if (p.endsWith("/user")) return [200, { email: "demo@akkuscan.app" }];
      return [200, {}];
    }
    if (p.endsWith("/rpc/app_ingest")) {
      const s = b.p_payload.scan, m = b.p_payload.meta || {};
      let bat = DB.bats.find(x => x.eserial == s.eserial);
      if (!bat) { bat = { id: uid("bat"), org_id: ORG, eserial: s.eserial, type_code: s.type_code, model: s.model, capacity_ah: s.capacity_ah, name: null,
        location: m.loc || null, comment: null, retired: false, group_id: null, created_at: iso(now()) }; DB.bats.push(bat); }
      const r = { id: uid("r"), org_id: ORG, battery_id: bat.id, measured_at: iso(m.ts || now()), health: s.health, status: s.status, source: b.p_source || "app-ble",
        location: m.loc || null, comment: m.comment || null, pack_v: s.pack_v, scan: s };
      DB.readings.push(r); return [200, { ok: true, duplicate: false, reading_id: r.id, battery_id: bat.id }];
    }
    if (p.endsWith("/rpc/bridge_register")) return [200, { ok: true, token: "demo-token" }];
    if (p.endsWith("/rpc/is_admin")) return [200, false];
    if (p.endsWith("/rpc/delete_my_account")) return [200, { ok: true }];
    if (p.endsWith("/orgs")) return [200, [{ id: ORG, name: "Demo Elektro GmbH" }]];
    if (p.endsWith("/battery_overview")) return [200, overview()];
    const table = p.endsWith("/battery_groups") ? DB.groups : p.endsWith("/batteries") ? DB.bats : p.endsWith("/readings") ? DB.readings : null;
    if (!table) return [200, []];
    const id = eq(u, "id");
    if (method == "GET") {
      let rows = table.filter(x => (!id || x.id == id) && (!eq(u, "eserial") || x.eserial == eq(u, "eserial")) && (!eq(u, "battery_id") || x.battery_id == eq(u, "battery_id")));
      if (table == DB.readings) rows = rows.slice().sort((x, y) => y.measured_at.localeCompare(x.measured_at));
      if (table == DB.groups) rows = rows.slice().sort((x, y) => x.sort - y.sort);
      return [200, rows];
    }
    if (method == "POST") { table.push(Object.assign({ id: uid("g"), org_id: ORG, icon: "folder", sort: table.length }, b)); return [201, null]; }
    if (method == "PATCH") { table.filter(x => x.id == id).forEach(x => Object.assign(x, b)); return [204, null]; }
    if (method == "DELETE") { if (table == DB.groups) DB.bats.forEach(x => { if (x.group_id == id) x.group_id = null; });
      const i = table.findIndex(x => x.id == id); if (i >= 0) table.splice(i, 1); return [204, null]; }
    return [200, []];
  }
  const realFetch = window.fetch.bind(window);
  window.fetch = async function (input, init) {
    const url = typeof input === "string" ? input : input.url;
    if (url.startsWith(SBURL)) {
      init = init || {}; await new Promise(s => setTimeout(s, 120));
      const [st, obj] = cloud((init.method || "GET").toUpperCase(), new URL(url), init.body);
      return new Response(obj == null ? null : JSON.stringify(obj), { status: st, headers: { "Content-Type": "application/json" } });
    }
    return realFetch(input, init);
  };

  // ------------------------------------------------------------ demo bar
  document.addEventListener("DOMContentLoaded", () => {
    const st = document.createElement("style");
    st.textContent = `#demobar{position:fixed;left:0;right:0;bottom:0;z-index:55;background:#ff7a00;color:#111;font:600 13.5px/1.35 system-ui,sans-serif;
      padding:9px 14px calc(9px + env(safe-area-inset-bottom));display:flex;gap:10px;align-items:center;justify-content:center;flex-wrap:wrap;box-shadow:0 -6px 20px rgba(0,0,0,.35)}
      #demobar a{color:#111;background:#fff;border-radius:999px;padding:4px 12px;text-decoration:none;font-weight:800}body{padding-bottom:64px}`;
    document.head.appendChild(st);
    document.body.insertAdjacentHTML("beforeend", '<div id="demobar"><span>DEMO – Beispieldaten, es wird nichts gespeichert. Einfach auf „Verbinden“ tippen und auslesen.</span><a href="/app/">Demo beenden</a></div>');
  });
})();
