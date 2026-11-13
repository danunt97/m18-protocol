/* Bluetooth adapter for the M18 app on akkuscan.app/app/.
 * The app IS the bridge's own web page (esp32-bridge/m18_bridge/web_ui.h). On the bridge it talks HTTP;
 * here this script answers the page's fetch("/api/...") and fetch("/cmd") calls over Bluetooth LE
 * (Nordic UART service, command "API <GET|POST> <path> <k=v&...>", one reply line "OK {json}" / "ERR <code> {json}").
 * esp32-bridge/tools/gen_app.py puts it in front of the page's own script -> webble/app.html. */
(function () {
  "use strict";
  const NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
  const NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  const enc = new TextEncoder();
  let dec = new TextDecoder(), dev = null, rxc = null, inbuf = "", pend = null, queue = Promise.resolve();
  const ble = window.M18_BLE = { connected: false };

  // ---------------------------------------------------------------- transport
  function onChunk(dv) {
    inbuf += dec.decode(dv, { stream: true });
    let i;
    while ((i = inbuf.indexOf("\n")) >= 0) {
      const line = inbuf.slice(0, i).replace(/\r$/, "");
      inbuf = inbuf.slice(i + 1);
      if (line.startsWith("PROG ")) continue;
      if (pend) { const p = pend; pend = null; clearTimeout(p.timer); p.res(line); }
    }
  }
  async function writeLine(s) {
    const b = enc.encode(s);
    for (let o = 0; o < b.length; o += 100) {
      const part = b.slice(o, o + 100);
      if (rxc.writeValueWithResponse) await rxc.writeValueWithResponse(part); else await rxc.writeValue(part);
    }
  }
  // One command at a time, exactly one reply line each.
  function cmd(line, timeout) {
    const run = () => new Promise((res, rej) => {
      if (!rxc) { rej(new TypeError("Nicht verbunden")); return; }
      pend = { res, rej, timer: setTimeout(() => { pend = null; rej(new TypeError("Keine Antwort über Bluetooth")); }, timeout || 20000) };
      writeLine(line + "\n").catch(e => { if (pend) { clearTimeout(pend.timer); pend = null; } rej(e); });
    });
    const p = queue.then(run, run);
    queue = p.catch(() => {});
    return p;
  }
  ble.cmd = cmd;

  // ------------------------------------------------------- fetch over Bluetooth
  const realFetch = window.fetch.bind(window);
  const json = (obj, status) => new Response(JSON.stringify(obj), { status, headers: { "Content-Type": "application/json" } });
  window.fetch = async function (input, init) {
    init = init || {};
    const url = typeof input === "string" ? input : input.url;
    if (!(url.startsWith("/api/") || url === "/cmd")) return realFetch(input, init);
    if (!ble.connected) throw new TypeError("Nicht verbunden");
    const method = (init.method || "GET").toUpperCase();
    let body = init.body || "";
    if (body instanceof URLSearchParams) body = body.toString();
    if (typeof body !== "string") return json({ ok: false, error: "Über Bluetooth nicht möglich" }, 501);
    if (url === "/cmd") {
      const out = [];
      for (const l of body.split("\n").map(s => s.trim()).filter(Boolean))
        out.push(await cmd(l, /^SCAN/i.test(l) ? 180000 : 20000));
      return new Response(out.join("\n") + "\n", { status: 200, headers: { "Content-Type": "text/plain" } });
    }
    if (url.startsWith("/api/update") || url.startsWith("/api/log.txt"))
      return json({ ok: false, error: "Nur im WLAN der Brücke möglich" }, 501);
    const r = await cmd("API " + method + " " + url + (body ? " " + body : ""), 30000);
    if (/^ERR AUTH/.test(r)) { askPassword(); return json({ ok: false, error: "Passwort nötig" }, 401); }
    const m = r.match(/^(OK|ERR)(?: (\d{3}))? ?([\s\S]*)$/);
    let status = 400, text = r;
    if (m) { status = m[1] === "OK" ? 200 : (+m[2] || 400); text = m[3]; }
    if (!/^[{[]/.test(text)) text = JSON.stringify({ ok: false, error: r.replace(/^ERR\s*/, "") });
    return new Response(text, { status, headers: { "Content-Type": "application/json" } });
  };

  // --------------------------------------------------------------- connection
  // The connect card belongs to the tabs that talk to the bridge; the Flotte tab (account, fleet,
  // password reset) works without it – there it only appears after tapping "Verbinden".
  let cardOnCloud = false;
  function syncCard() {
    const c = document.getElementById("blecard"), cloud = document.getElementById("tab-cloud");
    if (c) c.hidden = ble.connected || (!!cloud && !cloud.hidden && !cardOnCloud);
    if (c) c.classList.toggle("rc", !!ble.reconnecting);   // while reconnecting only the status line, not the instructions
  }
  // The module restarts whenever it is moved to the next battery (it is powered by the battery). The phone keeps the
  // paired device: on an unexpected disconnect we reconnect on our own until it is back (no device chooser again).
  let userClosed = false, reconnecting = false, lastPw = "";
  const hooked = new WeakSet();
  const emit = () => { try { window.dispatchEvent(new Event("m18-ble")); } catch (e) {} };
  const withTimeout = (p, ms) => Promise.race([p, new Promise((_, rej) => setTimeout(() => rej(new Error("Zeitüberschreitung")), ms))]);
  async function openGatt() {
    const g = await withTimeout(dev.gatt.connect(), 12000), svc = await g.getPrimaryService(NUS);
    rxc = await svc.getCharacteristic(NUS_RX);
    const txc = await svc.getCharacteristic(NUS_TX);
    dec = new TextDecoder(); inbuf = "";
    if (!hooked.has(txc)) { hooked.add(txc); txc.addEventListener("characteristicvaluechanged", e => onChunk(e.target.value)); }
    await txc.startNotifications();
    let r = await cmd("API GET /api/info");
    if (/^ERR AUTH/.test(r) && lastPw && /^OK/.test(await cmd("AUTH " + lastPw).catch(() => "ERR"))) r = await cmd("API GET /api/info");
    return r;
  }
  function useDevice(d) {
    dev = d;
    if (!hooked.has(dev)) { hooked.add(dev); dev.addEventListener("gattserverdisconnected", onDisconnected); }
    try { localStorage.setItem("m18.ble.dev", JSON.stringify({ id: dev.id, name: dev.name || "" })); } catch (e) {}
  }
  async function afterOpen(r) {
    if (/^ERR AUTH/.test(r)) { await askPassword(); return; }
    if (/^ERR UNKNOWN API/.test(r)) { setMsg("Die Firmware der Brücke ist zu alt für diese App – bitte über den Web-Flasher auf den neuesten Stand bringen.", true); disconnect(); return; }
    ready();
  }
  async function connect() {
    cardOnCloud = true; syncCard();
    if (!navigator.bluetooth) { showNoBt(); return; }
    try { if (navigator.bluetooth.getAvailability && !(await navigator.bluetooth.getAvailability())) {
      setMsg("Bluetooth ist am Handy ausgeschaltet. Bitte in den Einstellungen Bluetooth einschalten und nochmal auf „Verbinden“ tippen.", true); return; } } catch (e) {}
    let d;
    try { d = await navigator.bluetooth.requestDevice({ filters: [{ services: [NUS] }] }); }
    catch (e) {
      if (e.name === "NotFoundError") return;   // Auswahl abgebrochen
      setMsg(/adapter|not available|unavailable|disabled/i.test(e.message)
        ? "Bluetooth ist am Handy ausgeschaltet. Bitte in den Einstellungen Bluetooth einschalten und nochmal auf „Verbinden“ tippen."
        : "Bluetooth: " + e.message, true); return; }
    stopReconnect(); userClosed = false; useDevice(d);
    setBar("verbinde mit " + (dev.name || "Brücke") + " …");
    try { await afterOpen(await openGatt()); }
    catch (e) { setMsg("Verbindung fehlgeschlagen: " + e.message, true); disconnect(); }
  }
  function ready() {
    stopReconnect();
    ble.connected = true;
    setBar(null); setMsg("", false);
    syncCard(); emit();
    if (window.M18_START) window.M18_START();
  }
  function disconnect() {   // user taps "Trennen": no automatic reconnect
    userClosed = true; stopReconnect();
    if (dev && dev.gatt.connected) dev.gatt.disconnect(); else onDisconnected();
  }
  function onDisconnected() {
    const was = ble.connected;
    rxc = null;
    ble.connected = false;
    if (pend) { clearTimeout(pend.timer); pend.rej(new TypeError("Verbindung getrennt")); pend = null; }
    setBar(null); syncCard(); emit();
    if (!userClosed && dev && was) startReconnect();
    else if (!reconnecting) setMsg(dev && was ? "Verbindung zu " + (dev.name || "der Brücke") + " getrennt." : "", false);
  }
  let rcTimer = null, rcUntil = 0;
  function stopReconnect() { reconnecting = false; ble.reconnecting = false; clearTimeout(rcTimer); syncCard(); emit(); }
  function startReconnect(quiet) {
    if (reconnecting || !dev) return;
    reconnecting = true; ble.reconnecting = true; rcUntil = Date.now() + 10 * 60 * 1000; syncCard(); emit();
    const name = dev.name || "das Modul";
    setBar("verbinde neu …");
    if (!quiet) setMsg("", false);
    const m = document.getElementById("blemsg");
    if (m) m.innerHTML = `<div class="box info" id="blerc"><span class="spin" style="display:inline-block;vertical-align:middle;margin-right:6px"></span>
      <b>${esc(name)} wird automatisch wieder verbunden …</b><br>Das passiert z. B., wenn das Modul auf den nächsten Akku gesteckt wird.
      Sobald es wieder Strom hat, geht es von selbst weiter. <a href="#" id="blercstop" style="color:inherit">Abbrechen</a></div>`;
    const st = document.getElementById("blercstop"); if (st) st.onclick = e => { e.preventDefault(); userClosed = true; stopReconnect(); setMsg("", false); setBar(null); };
    const tryOnce = async () => {
      if (!reconnecting) return;
      if (Date.now() > rcUntil) { stopReconnect(); setBar(null); setMsg(name + " wurde nicht wiedergefunden. Bitte auf „Verbinden“ tippen.", true); return; }
      try { const r = await openGatt(); if (!reconnecting) return; await afterOpen(r); return; }
      catch (e) { try { if (dev.gatt.connected) dev.gatt.disconnect(); } catch (x) {} }
      rcTimer = setTimeout(tryOnce, 2000);
    };
    rcTimer = setTimeout(tryOnce, 1500);
  }
  ble.connect = connect;
  ble.disconnect = disconnect;
  ble.reconnecting = false;
  // After reloading the page: Chrome can hand back the module that was allowed before – then reconnect without asking.
  async function resume() {
    if (!navigator.bluetooth || !navigator.bluetooth.getDevices) return;
    let saved = null; try { saved = JSON.parse(localStorage.getItem("m18.ble.dev")); } catch (e) {}
    if (!saved) return;
    try { const list = await navigator.bluetooth.getDevices(); const d = list.find(x => x.id === saved.id) || list.find(x => saved.name && x.name === saved.name);
      if (d && !ble.connected) { useDevice(d); userClosed = false; startReconnect(true); } } catch (e) {}
  }
  function askPassword() {
    return new Promise(res => {
      const box = document.getElementById("bleauth");
      box.hidden = false;
      document.getElementById("blepw").focus();
      document.getElementById("blepwgo").onclick = async () => {
        const pw = document.getElementById("blepw").value;
        const r = await cmd("AUTH " + pw).catch(e => "ERR " + e.message);
        if (!/^OK/.test(r)) { document.getElementById("blepwerr").textContent = r.replace(/^ERR\s*/, ""); return; }
        lastPw = pw;
        box.hidden = true;
        ready();
        res();
      };
    });
  }
  const esc = t => String(t).replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

  // ----------------------------------------------------------------------- UI
  const CSS = `#blebar{display:flex;align-items:center;gap:8px;margin-left:auto}
#blebtn{padding:7px 12px}#blestate{color:var(--muted);font-size:12px}
@media(max-width:560px){#pill{display:none}}
.ble-only-wlan{opacity:.55}
#blecard.rc > ol,#blecard.rc > .hint,#blecard.rc #blebig{display:none}
#nobt{margin-top:14px;border:2px solid var(--orange,#ff7a00);border-radius:16px;padding:18px;background:rgba(255,122,0,.08)}
#nobt h3{font-size:20px;line-height:1.25;margin:0 0 8px}
#nobt p{margin:0 0 12px;line-height:1.5}
#nobt ol{list-style:none;margin:14px 0 0;padding:0;counter-reset:n}
#nobt li{position:relative;padding:0 0 18px 44px;line-height:1.45}
#nobt li::before{counter-increment:n;content:counter(n);position:absolute;left:0;top:0;width:30px;height:30px;border-radius:50%;
  background:var(--accent,#e1251b);color:#fff;font-weight:800;display:grid;place-items:center}
#nobt li b{display:block;font-size:16.5px;margin-bottom:6px}
.nb-app{display:flex;align-items:center;gap:12px;text-decoration:none;color:inherit;border:1px solid var(--line,#30353c);
  background:var(--panel,#22262b);border-radius:14px;padding:10px 12px;max-width:380px}
.nb-app svg{flex:0 0 52px;width:52px;height:52px}
.nb-app span{flex:1;min-width:0;line-height:1.3}.nb-name{display:block;font-size:17px}.nb-app small{display:block;color:var(--muted,#a3a9b1);font-size:12.5px}
.nb-get{background:#0a84ff;color:#fff;border-radius:999px;padding:7px 14px;font-weight:700;font-size:14px;white-space:nowrap}
.nb-url{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.nb-url code{font:700 17px/1 ui-monospace,Menlo,monospace;background:var(--panel,#22262b);border:1px solid var(--line,#30353c);
  border-radius:10px;padding:10px 12px;user-select:all;-webkit-user-select:all}
.nb-alt{color:var(--muted,#a3a9b1);font-size:13.5px;margin-top:4px}`;
  function setBar(text) {
    const b = document.getElementById("blebtn"), s = document.getElementById("blestate");
    if (!b) return;
    s.textContent = text || (ble.connected ? (dev && dev.name) || "verbunden" : "");
    b.textContent = ble.connected ? "Trennen" : "Verbinden";
    b.className = "btn" + (ble.connected ? "" : " primary");
  }
  function setMsg(t, err) {
    const m = document.getElementById("blemsg");
    if (m) m.innerHTML = t ? `<div class="box ${err ? "err" : "info"}">${t.replace(/</g, "&lt;")}</div>` : "";
  }
  // No Web Bluetooth in this browser: say clearly what to do, with one tap to the right app.
  const APP_URL = "https://akkuscan.app/app/";
  const BLUEFY = "https://apps.apple.com/app/id1492822055";
  const BLUEFY_ICON = '<svg viewBox="0 0 64 64" aria-hidden="true"><defs><linearGradient id="nbg" x1="0" y1="0" x2="0" y2="1">' +
    '<stop offset="0" stop-color="#3aa0ff"/><stop offset="1" stop-color="#0a5ad6"/></linearGradient></defs>' +
    '<rect width="64" height="64" rx="14" fill="url(#nbg)"/><path d="M22 22l20 18-10 9V15l10 9-20 18" fill="none" stroke="#fff" stroke-width="4.5" stroke-linecap="round" stroke-linejoin="round"/></svg>';
  function showNoBt() {
    const ua = navigator.userAgent;
    const ios = /iPhone|iPad|iPod/.test(ua) || (/Macintosh/.test(ua) && navigator.maxTouchPoints > 1);
    const android = /Android/.test(ua);
    let html;
    if (ios) html = `<h3>Auf dem iPhone brauchst du die kostenlose App „Bluefy“.</h3>
      <p>Safari und die anderen Browser auf dem iPhone können sich nicht per Bluetooth mit dem Modul verbinden. Die App „Bluefy“ kann das – so geht's:</p>
      <ol>
        <li><b>Bluefy kostenlos laden</b>
          <a class="nb-app" href="${BLUEFY}" target="_blank" rel="noopener">${BLUEFY_ICON}<span><strong class="nb-name">Bluefy</strong><small>kostenlos</small></span><span class="nb-get">Laden</span></a></li>
        <li><b>Bluefy öffnen und diese Adresse eingeben</b>
          <span class="nb-url"><code>akkuscan.app/app</code><button class="btn" id="nbcopy" type="button">Adresse kopieren</button></span></li>
        <li><b>Auf „Verbinden“ tippen – fertig.</b>Dein Konto und deine Flotte sind dort genauso da.</li>
      </ol>
      <p class="nb-alt">Ohne Bluefy geht es auch über WLAN: In den iPhone-Einstellungen das WLAN des Moduls („M18-Bridge-…“) wählen – dann öffnet sich die Seite des Moduls in Safari.</p>`;
    else if (android) html = `<h3>Bitte öffne diese Seite in Google Chrome.</h3>
      <p>Dieser Browser kann sich nicht per Bluetooth mit dem Modul verbinden. Chrome kann das – und ist auf fast jedem Android-Handy schon drauf.</p>
      <a class="btn primary big" href="intent://akkuscan.app/app/#Intent;scheme=https;package=com.android.chrome;end">In Chrome öffnen</a>
      <p class="nb-alt" style="margin-top:12px">Klappt der Knopf nicht: Chrome öffnen und <b>akkuscan.app/app</b> eingeben.</p>`;
    else html = `<h3>Bitte öffne diese Seite in Google Chrome oder Microsoft Edge.</h3>
      <p>Dieser Browser (z. B. Safari oder Firefox) kann sich nicht per Bluetooth mit dem Modul verbinden. Am PC oder Mac gehen Chrome und Edge.</p>
      <span class="nb-url"><code>akkuscan.app/app</code><button class="btn" id="nbcopy" type="button">Adresse kopieren</button></span>`;
    const m = document.getElementById("blemsg"); if (!m) return;
    m.innerHTML = `<div id="nobt" role="alert">${html}</div>`;
    const big = document.getElementById("blebig"), steps = document.querySelector("#blecard > ol"), hint = document.querySelector("#blecard > .hint");
    if (big) big.hidden = true; if (steps) steps.hidden = true; if (hint) hint.hidden = true;
    const c = document.getElementById("nbcopy");
    if (c) c.onclick = async () => {
      try { await navigator.clipboard.writeText(APP_URL); }
      catch (e) { const r = document.createRange(); r.selectNodeContents(document.querySelector("#nobt code")); const s = getSelection(); s.removeAllRanges(); s.addRange(r); document.execCommand("copy"); }
      c.textContent = "Kopiert ✓"; setTimeout(() => { c.textContent = "Adresse kopieren"; }, 2500);
    };
  }
  document.addEventListener("DOMContentLoaded", () => {
    const st = document.createElement("style"); st.textContent = CSS; document.head.appendChild(st);
    const header = document.querySelector("header");
    const acct = document.getElementById("acct"), bar = '<span id="blebar"><span id="blestate"></span><button class="btn primary" id="blebtn">Verbinden</button></span>';
    if (acct) acct.insertAdjacentHTML("beforebegin", bar); else header.insertAdjacentHTML("beforeend", bar);
    document.getElementById("blebtn").onclick = () => ble.connected ? disconnect() : connect();
    const host = document.getElementById("hostline"); if (host) host.textContent = "Bluetooth-App";
    const pill = document.getElementById("pill"); if (pill) pill.hidden = true;
    document.querySelector("main").insertAdjacentHTML("afterbegin", `
<div class="card" id="blecard"><h2>Mit der Brücke verbinden</h2>
  <ol style="margin:4px 0 0 18px;line-height:1.7"><li>Brücke auf den Akku stecken (oder per USB versorgen).</li>
    <li><b>Verbinden</b> tippen und <b>M18-Bridge-…</b> wählen.</li><li><b>Akku auslesen</b> – alles Weitere wie auf der Brücke selbst.</li></ol>
  <button class="btn primary big" id="blebig" style="margin-top:12px">Verbinden</button><div id="blemsg"></div>
  <p class="hint">iPhone: diese Seite in der kostenlosen App <b>Bluefy</b> öffnen (Safari kann kein Bluetooth). Android: <b>Chrome</b>.
    Das WLAN der Brücke wird nicht gebraucht – das Handy behält sein Internet. Konto und Flotte (Tab Flotte) gehen auch ohne Brücke.</p></div>
<div class="card" id="bleauth" hidden><h2>Passwort der Brücke</h2><label>Passwort</label><input id="blepw" type="password" autocomplete="current-password">
  <button class="btn primary" id="blepwgo" style="margin-top:10px">Anmelden</button><div id="blepwerr" class="hint" style="color:var(--crit)"></div></div>`);
    document.getElementById("blebig").onclick = connect;
    // Only possible in the bridge's Wi-Fi: firmware upload and the log file download.
    const fw = document.getElementById("fwbtn");
    if (fw) { const card = fw.closest(".card"); card.classList.add("ble-only-wlan");
      card.insertAdjacentHTML("afterbegin", '<p class="hint" style="margin-top:0">Firmware-Update geht nur im WLAN der Brücke (oder über den Web-Flasher per USB).</p>');
      fw.disabled = true; const f = document.getElementById("fwfile"); if (f) f.disabled = true; }
    document.querySelectorAll('a[href="/api/log.txt"]').forEach(a => {
      a.removeAttribute("download");
      a.onclick = async e => { e.preventDefault(); if (!ble.connected) return;
        const r = await cmd("LOG 250"); const d = JSON.parse(r.slice(3));
        const blob = new Blob([d.lines.join("\n") + "\n"], { type: "text/plain" }), u = document.createElement("a");
        u.href = URL.createObjectURL(blob); u.download = "m18-bruecke-protokoll.txt"; document.body.appendChild(u); u.click();
        setTimeout(() => { URL.revokeObjectURL(u.href); u.remove(); }, 500); };
    });
    if (!navigator.bluetooth) showNoBt();
    else resume();
    const tr = document.createElement("script"); tr.src = "/track.js"; tr.defer = true; document.head.appendChild(tr);   // anonyme Seitenstatistik
    const cloud = document.getElementById("tab-cloud");
    if (cloud) new MutationObserver(() => { cardOnCloud = false; syncCard(); }).observe(cloud, { attributes: true, attributeFilter: ["hidden"] });
    syncCard();
  });
})();
