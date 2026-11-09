// Web interface of the M18 bridge (served from flash, no external resources).
// Tabs: Akku (direct read-out), WLAN (setup in the customer's network), Gerät (status, console).
#pragma once

static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>M18 Akku-Brücke</title>
<style>
:root{--bg:#1b1d20;--panel:#23262a;--panel2:#1a1c1f;--raise:#2c3035;--line:#34383e;--text:#eceef0;--muted:#959ba3;--faint:#6b7178;
--accent:#e4002b;--good:#3fcf7f;--watch:#e7b43a;--weak:#f0703c;--crit:#ff4d5e;--info:#5aa2ff;color-scheme:dark}
@media (prefers-color-scheme:light){:root{--bg:#f3f4f6;--panel:#fff;--panel2:#f7f8fa;--raise:#eef0f3;--line:#dde0e5;--text:#17191c;--muted:#5e646c;--faint:#8a9098;
--good:#12935a;--watch:#b07d00;--weak:#c4501c;--crit:#d0142c;--info:#1f6fd6;color-scheme:light}}
*{box-sizing:border-box;margin:0;padding:0}[hidden]{display:none!important}
body{background:var(--bg);color:var(--text);font:14px/1.45 system-ui,"Segoe UI",sans-serif}
header{display:flex;align-items:center;gap:10px;padding:12px 16px;background:var(--panel);border-bottom:1px solid var(--line);flex-wrap:wrap}
.logo{background:var(--accent);color:#fff;font-weight:800;font-size:12px;border-radius:7px;padding:6px 7px}
header h1{font-size:16px;font-weight:650}header small{display:block;color:var(--muted);font-size:11.5px;font-weight:400}
.pill{margin-left:auto;font-size:12px;border-radius:20px;padding:3px 10px;background:var(--raise);color:var(--muted)}
.pill.ok{background:rgba(63,207,127,.14);color:var(--good)}.pill.ap{background:rgba(90,162,255,.15);color:var(--info)}
nav{display:flex;gap:4px;padding:10px 16px 0;max-width:820px;margin:0 auto}
nav button{flex:1;background:transparent;border:0;border-bottom:2px solid transparent;color:var(--muted);padding:9px;font:inherit;cursor:pointer}
nav button.on{color:var(--text);border-bottom-color:var(--accent)}
main{max-width:820px;margin:0 auto;padding:14px 16px 40px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:16px;margin-bottom:14px}
.card h2{font-size:12px;text-transform:uppercase;letter-spacing:.8px;color:var(--muted);margin-bottom:10px}
.btn{display:inline-flex;align-items:center;gap:8px;justify-content:center;background:var(--raise);border:1px solid var(--line);color:var(--text);
border-radius:8px;padding:9px 14px;font:inherit;cursor:pointer;text-decoration:none}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}.btn.big{padding:13px 22px;font-size:15px;width:100%}
.btn:disabled{opacity:.5}
input,select,textarea{width:100%;background:var(--panel2);border:1px solid var(--line);color:var(--text);border-radius:7px;padding:9px 10px;font:inherit}
label{display:block;color:var(--muted);font-size:12.5px;margin:10px 0 4px}
.hint{color:var(--muted);font-size:12.5px;margin-top:8px}.mono{font-family:ui-monospace,Consolas,monospace;font-size:12.5px}
.spin{width:16px;height:16px;border:2px solid rgba(255,255,255,.35);border-top-color:#fff;border-radius:50%;animation:s .8s linear infinite}@keyframes s{to{transform:rotate(360deg)}}
.hero{display:flex;gap:16px;align-items:center;flex-wrap:wrap}
.gauge{width:120px;height:120px;flex:none}.gauge .bg{stroke:var(--raise)}
.title b{font-size:18px;display:block}.title span{color:var(--muted)}
.badge{display:inline-block;border-radius:20px;padding:2px 9px;font-size:12px;font-weight:600;margin-top:4px}
.good{color:var(--good)}.watch{color:var(--watch)}.weak{color:var(--weak)}.crit{color:var(--crit)}.unknown{color:var(--muted)}
.badge.good{background:rgba(63,207,127,.13)}.badge.watch{background:rgba(231,180,58,.14)}.badge.weak{background:rgba(240,112,60,.15)}.badge.unknown{background:var(--raise)}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px;margin-top:14px}
.stat{background:var(--panel2);border:1px solid var(--line);border-radius:8px;padding:9px 11px}
.stat .k{color:var(--muted);font-size:12px}.stat .v{font-size:19px;font-weight:600;font-variant-numeric:tabular-nums}.stat .v small{font-size:12px;color:var(--muted);font-weight:400}
.cell{display:grid;grid-template-columns:54px 1fr 70px;gap:10px;align-items:center;margin:7px 0}
.track{display:block;height:12px;background:var(--raise);border-radius:6px;overflow:hidden}.fill{display:block;height:100%;background:linear-gradient(90deg,#1f7a3f,var(--good));border-radius:6px}
.cell .cv{text-align:right;font-variant-numeric:tabular-nums}.cell .lab{color:var(--muted)}
.row{display:flex;justify-content:space-between;gap:10px;padding:6px 0;border-bottom:1px solid var(--line)}.row:last-child{border:0}.row span:first-child{color:var(--muted)}
.part .p{color:var(--weak);font-weight:600}.part small{display:block;color:var(--faint)}
.nets{display:flex;flex-direction:column;gap:6px;margin-top:10px}
.net{display:flex;justify-content:space-between;align-items:center;background:var(--panel2);border:1px solid var(--line);border-radius:8px;padding:9px 11px;cursor:pointer}
.net:hover{border-color:var(--faint)}.bars{color:var(--muted);font-size:12px}
.box{border-radius:8px;padding:12px 14px;border:1px solid;margin-top:12px}
.box.ok{background:rgba(63,207,127,.12);border-color:rgba(63,207,127,.4)}.box.err{background:rgba(255,77,94,.12);border-color:rgba(255,77,94,.4)}
.box.info{background:rgba(90,162,255,.12);border-color:rgba(90,162,255,.4)}
.big-url{font-size:17px;font-weight:650;word-break:break-all}
pre{background:var(--panel2);border:1px solid var(--line);border-radius:8px;padding:10px;white-space:pre-wrap;word-break:break-all;font-size:12px;max-height:260px;overflow:auto}
.prog{height:6px;background:var(--raise);border-radius:3px;overflow:hidden;margin-top:10px}.prog i{display:block;height:100%;width:0;background:var(--accent);transition:width .3s}
.warn{color:var(--watch)}input[type=range]{padding:0}
.two{display:grid;grid-template-columns:1fr 1fr;gap:14px}@media(max-width:640px){.two{grid-template-columns:1fr}}
details.rg{border-top:1px solid var(--line)}details.rg summary{cursor:pointer;padding:10px 0;font-weight:600}
.reg{display:grid;grid-template-columns:1fr auto;gap:10px;padding:5px 0;border-top:1px solid var(--line);align-items:center}
.reg.h{grid-template-columns:minmax(96px,38%) 1fr auto}.reg .lab{color:var(--muted)}
.reg .lab small{display:block;color:var(--faint);font-size:10.5px;font-family:ui-monospace,Consolas,monospace}
.reg .rv{text-align:right;font-variant-numeric:tabular-nums;word-break:break-word}.faint{color:var(--faint)}
</style></head><body>
<header><span class="logo">M18</span><h1>Akku-Brücke<small id="hostline">…</small></h1><span class="pill" id="pill">…</span></header>
<nav><button data-tab="akku" class="on">Akku</button><button data-tab="verlauf">Verlauf <span id="newcount" class="pill ok" style="padding:0 7px;margin:0" hidden></span></button><button data-tab="cloud">Cloud <span id="qcount" class="pill" style="padding:0 7px;margin:0" hidden></span></button><button data-tab="wlan">WLAN</button><button data-tab="geraet">Gerät</button></nav>
<main>
<section id="tab-akku">
  <div class="card">
    <div class="two" style="gap:10px;margin-bottom:12px"><div><label style="margin-top:0">Standort / Kunde (optional)</label><input id="loc" maxlength="60" placeholder="z. B. Kunde Meier, Halle 2"></div>
      <div><label style="margin-top:0">Kommentar (optional)</label><input id="comment" maxlength="200" placeholder="z. B. Akku 3 aus Koffer"></div></div>
    <button class="btn primary big" id="readbtn">Akku auslesen</button>
    <div class="prog" id="prog" hidden><i></i></div>
    <p class="hint" id="readhint">Akku am Adapter anschließen und auslesen (dauert ca. 7 Sekunden).</p></div>
  <div id="result"></div>
</section>
<section id="tab-verlauf" hidden>
  <div class="card"><h2>Gespeicherte Messungen</h2><div id="hstats">…</div>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:12px">
      <button class="btn" id="hexport">Alle als JSON exportieren</button><button class="btn" id="hclear">Übernommene löschen</button></div>
    <p class="hint">Jede erfolgreiche Messung wird auf der Brücke gespeichert. Die PC-Software übernimmt sie in die Datenbank
      (Auslesen → Brücke verbinden → „Übernehmen“) – oder die JSON-Datei dort unter Einstellungen importieren.</p></div>
  <div class="card"><div id="hlist"></div><button class="btn" id="hmore" hidden style="margin-top:10px">Ältere laden</button></div>
  <div id="hview"></div>
</section>
<section id="tab-cloud" hidden>
  <div id="cloudbox"></div><div id="cloudview"></div>
</section>
<section id="tab-wlan" hidden>
  <div class="card"><h2>Verbindung</h2><div id="wifistate">…</div></div>
  <div class="card"><h2>Mit WLAN verbinden</h2><div id="lockbox"></div>
    <button class="btn" id="scanbtn">Netzwerke suchen</button><div class="nets" id="nets"></div>
    <form id="wifiform"><label>Netzwerk (SSID)</label><input name="ssid" id="ssid" required autocomplete="off">
      <label>Passwort</label><input name="pass" id="pass" type="password" autocomplete="off">
      <label>Gerätename im Netzwerk</label><input name="host" id="host" pattern="[a-zA-Z0-9-]{1,30}">
      <p class="hint">Danach erreichbar unter <b class="mono" id="hostpreview"></b> – die PC-Software findet das Gerät auch automatisch.</p>
      <button class="btn primary big" style="margin-top:12px">Verbinden</button></form>
    <div id="wifiprogress"></div></div>
  <div class="card"><h2>Zurücksetzen</h2><button class="btn" id="forget">WLAN-Daten löschen</button>
    <p class="hint">Alternativ die BOOT-Taste 5 Sekunden gedrückt halten. Danach startet der Einrichtungsmodus (WLAN „M18-Bridge-…“).</p></div>
</section>
<section id="tab-geraet" hidden>
  <div class="card"><h2>Cloud</h2><div id="cloudlink"><p class="hint" style="margin-top:0">…</p></div></div>
  <div class="two"><div class="card"><h2>Status</h2><div id="devinfo"></div></div>
  <div class="card"><h2>Verbindungstest</h2><button class="btn" id="testbtn">Akku testen</button>
    <p class="hint">Sendet Reset und prüft, ob der Akku mit 0xAA antwortet.</p><div id="testres"></div></div></div>
  <div class="two"><div class="card"><h2>Status-LED</h2>
    <label>LED-Pin</label><select id="ledpin"><option value="48">GPIO 48 (DevKitC-1 v1.0, die meisten N16R8)</option>
      <option value="38">GPIO 38 (DevKitC-1 v1.1)</option><option value="-1">aus</option><option value="custom">anderer Pin …</option></select>
    <input id="ledcustom" type="number" min="1" max="48" placeholder="GPIO-Nummer" hidden style="margin-top:6px">
    <label>Helligkeit</label><input id="ledbright" type="range" min="0" max="255">
    <div style="display:flex;gap:8px;margin-top:10px"><button class="btn" id="ledtest">Testen (rot, grün, blau, weiß)</button><button class="btn primary" id="ledsave">Speichern</button></div>
    <p class="hint">Grün = im WLAN · Blau blinkt = Einrichtungsmodus · Gelb = liest Akku · Grün kurz = gelesen · Rot blinkt = Fehler ·
      Weiß blinkt = PC liest über die Brücke · Türkis = Einstellungen entsperrt · Lila = Firmware-Update</p></div>
  <div class="card"><h2>Firmware-Update</h2><div id="otalock"></div>
    <input type="file" id="fwfile" accept=".bin"><button class="btn primary" id="fwbtn" style="margin-top:10px">Firmware hochladen</button>
    <div class="prog" id="fwprog" hidden><i></i></div><div id="fwres"></div>
    <p class="hint">.bin aus der Arduino IDE: <i>Sketch → Kompilierte Binärdatei exportieren</i> (die Datei ohne „bootloader“/„merged“ im Namen).</p></div></div>
  <div class="card"><h2>Energiesparen</h2><div id="sleepstate"></div>
    <label>Tiefschlaf nach Ruhe von</label><select id="sleepmin"><option value="0">nie</option><option value="10">10 Minuten</option>
      <option value="30">30 Minuten</option><option value="60">1 Stunde</option><option value="240">4 Stunden</option></select>
    <button class="btn" id="sleepsave" style="margin-top:10px">Speichern</button>
    <p class="hint">Im Aufsteckgehäuse wird die Brücke aus dem Akku versorgt. Wach zieht sie ca. 30–40 mA – nach der eingestellten
      Ruhezeit schläft sie (J2 bleibt auf Low). <b>Aufwecken: BOOT-Taste drücken.</b> Der Spannungswandler zieht auch im Schlaf noch
      einige mA – Gehäuse nach Gebrauch abziehen oder einen Schalter einbauen.</p></div>
  <div class="card"><h2>Zugriffsschutz</h2><div id="passstate"></div>
    <label>Neues Passwort (leer lassen = Schutz aus)</label><input id="newpass" type="password" autocomplete="new-password" maxlength="63">
    <label>Wiederholen</label><input id="newpass2" type="password" autocomplete="new-password" maxlength="63">
    <button class="btn primary" id="passbtn" style="margin-top:10px">Speichern</button>
    <p class="hint">Schützt Weboberfläche und PC-Zugriff (Benutzer „m18“). Ab 8 Zeichen gilt es auch für das Einrichtungs-WLAN.
      Vergessen? BOOT-Taste 5 s halten setzt WLAN und Passwort zurück.</p></div>
  <div class="card"><h2>Protokoll</h2>
    <p class="hint" style="margin-top:0">Was Brücke und Akku getan haben: Start, WLAN, Befehle vom PC, jedes Auslesen mit Ablauf
      (Wecken, abgelehnte Register, Wiederholungen), Updates, Neustarts. Bleibt nach einem Neustart erhalten.</p>
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin:8px 0">
      <button class="btn" id="logref">Aktualisieren</button><a class="btn" href="/api/log.txt" download>Komplett herunterladen</a>
      <button class="btn" id="logclr">Löschen</button>
      <label style="display:flex;align-items:center;gap:6px;margin:0"><input type="checkbox" id="logwarn" style="width:auto"> nur Warnungen/Fehler</label></div>
    <pre id="logbox" style="max-height:420px">…</pre></div>
  <div class="card"><h2>Befehle</h2><textarea id="cmd" rows="4">PING
RESET
READ 0x400A 10
IDLE</textarea><button class="btn" id="cmdbtn" style="margin-top:8px">Senden</button><pre id="cmdres" hidden></pre>
    <p class="hint">PING · RESET · IDLE · READ &lt;addr&gt; &lt;len&gt; · NOTE &lt;addr&gt; &lt;byte&gt; · SCAN · INFO · HSTAT · HLIST · HGET &lt;id&gt;</p></div>
</section>
</main>
<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const esc=v=>v==null?"":String(v).replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
const nf=(v,d=0)=>v==null?"–":Number(v).toLocaleString("de-DE",{minimumFractionDigits:d,maximumFractionDigits:d});
const fd=s=>s?new Date(s+"T00:00:00Z").toLocaleDateString("de-DE"):"–";
/*REGS-BEGIN (generated by esp32-bridge/tools/gen_regs.py)*/
const REG_GROUPS=[["Grunddaten",false,""],["Aktuelle Messwerte",false,""],["Zähler und Zeiten",false,""],["Entladezeit nach Strom (10-A-Stufen)",true,"s"],["Entladezeit nach Strom (5-A-Stufen)",true,"s"],["Ladungen nach Akkuspannung",true,""],["Ladungen nach Temperatur",true,""],["Einfache Ladungen nach Dauer",true,""],["Redlink-Ladungen nach Dauer",true,""],["Weitere Werte (Bedeutung unbekannt)",false,""]];
const REGS=[[0,2,"u","Zelltyp",0],[2,2,"u","Unbekannt (immer 0)",0],[4,5,"s","Typ und Seriennummer",0],[13,4,"u","Unbekannt (4. Code?)",0],[17,4,"d","Herstelldatum",0],[21,4,"d","Erste Ladung (Forge)",0],[25,4,"d","Letzte Ladung (Forge)",0],[35,20,"a","Notiz",0],[55,4,"d","Datum im Akku",0],[105,2,"u","Unbekannt (immer 2)",0],[123,1,"u","Unbekannt (immer 0)",0],[16384,4,"u","Unbekannt (Forge)",1],[16394,10,"c","Zellspannungen",1],[16404,2,"T","Temperatur (Fühler, nicht Forge)",1],[16406,2,"u","Unbekannt (Forge)",1],[16409,2,"u","Unbekannt (Forge)",1],[16411,2,"u","Unbekannt (Forge)",1],[16413,2,"u","Unbekannt (Forge)",1],[16415,2,"F","Temperatur (Forge)",1],[24576,2,"u","Unbekannt (Forge)",1],[24578,2,"u","Unbekannt (Forge)",1],[24580,4,"u","Unbekannt (Forge)",1],[24584,4,"u","Unbekannt (Forge)",1],[24588,2,"u","Unbekannt (Forge)",1],[36864,4,"d","Erste Ladung (gerundet)",2],[36868,4,"d","Letzte Werkzeugnutzung (gerundet)",2],[36872,4,"d","Letzte Ladung (gerundet)",2],[36876,4,"d","Unbekanntes Datum (meist leer)",2],[36880,2,"u","Tage seit Erstladung",2],[36882,4,"u","Entladung gesamt (As)",2],[36886,4,"u","Entladung gesamt (Ws)",2],[36890,4,"u","Ladungen gesamt",2],[36894,2,"u","Einfache Ladungen (ohne Redlink)",2],[36896,2,"u","Redlink-Ladungen",2],[36898,2,"u","Abgeschlossene Ladungen (?)",2],[36900,4,"t","Ladezeit gesamt",2],[36904,4,"t","Zeit voll im Ladegerät",2],[36908,2,"u","Unbekannt (weiterer Tiefentlade-Zähler?)",2],[36910,2,"u","Ladung begonnen mit Zelle < 2,5 V",2],[36912,2,"u","Leer gefahren",2],[36914,2,"u","Überhitzung am Werkzeug (> 10 A)",2],[36916,2,"u","Überstrom (?)",2],[36918,2,"u","Unterspannungs-Ereignisse",2],[36920,2,"u","Unterspannung kurz (4 LEDs blinken?)",2],[36922,2,"u","10–20 A",3],[36924,2,"u","20–30 A",3],[36926,2,"u","30–40 A",3],[36928,2,"u","40–50 A",3],[36930,2,"u","50–60 A",3],[36932,2,"u","60–70 A",3],[36934,2,"u","70–80 A",3],[36936,2,"u","80–90 A",3],[36938,2,"u","90–100 A",3],[36940,2,"u","100–110 A",3],[36942,2,"u","110–120 A",3],[36944,2,"u","120–130 A",3],[36946,2,"u","130–140 A",3],[36948,2,"u","140–150 A",3],[36950,2,"u","150–160 A",3],[36952,2,"u","160–170 A",3],[36954,2,"u","170–180 A",3],[36956,2,"u","180–190 A",3],[36958,2,"u","190–200 A",3],[36960,2,"u","200–210 A",3],[36962,2,"u","5–10 A",4],[36964,2,"u","10–15 A",4],[36966,2,"u","15–20 A",4],[36968,2,"u","20–25 A",4],[36970,2,"u","25–30 A",4],[36972,2,"u","30–35 A",4],[36974,2,"u","35–40 A",4],[36976,2,"u","40–45 A",4],[36978,2,"u","45–50 A",4],[36980,2,"u","50–55 A",4],[36982,2,"u","55–60 A",4],[36984,2,"u","60–65 A",4],[36986,2,"u","65–70 A",4],[36988,2,"u","70–75 A",4],[36990,2,"u","75–80 A",4],[36992,2,"u","80–85 A",4],[36994,2,"u","85–90 A",4],[36996,2,"u","90–95 A",4],[36998,2,"u","95–100 A",4],[37000,2,"u","100–105 A",4],[37002,2,"u","105–110 A",4],[37004,2,"u","110–115 A",4],[37006,2,"u","115–120 A",4],[37008,2,"u","120–125 A",4],[37010,2,"u","125–130 A",4],[37012,2,"u","130–135 A",4],[37014,2,"u","135–140 A",4],[37016,2,"u","140–145 A",4],[37018,2,"u","145–150 A",4],[37020,2,"u","150–155 A",4],[37022,2,"u","155–160 A",4],[37024,2,"u","160–165 A",4],[37026,2,"u","165–170 A",4],[37028,2,"u","170–175 A",4],[37030,2,"u","175–180 A",4],[37032,2,"u","180–185 A",4],[37034,2,"u","185–190 A",4],[37036,2,"u","190–195 A",4],[37038,2,"u","195–200 A",4],[37040,2,"u","über 200 A",4],[37042,2,"u","Beginn bei < 17 V",5],[37044,2,"u","Beginn bei 17–18 V",5],[37046,2,"u","Beginn bei 18–19 V",5],[37048,2,"u","Beginn bei 19–20 V",5],[37050,2,"u","Beginn bei über 20 V",5],[37052,2,"u","Ende bei < 17 V",5],[37054,2,"u","Ende bei 17–18 V",5],[37056,2,"u","Ende bei 18–19 V",5],[37058,2,"u","Ende bei 19–20 V",5],[37060,2,"u","Ende bei über 20 V",5],[37062,2,"u","Beginn −30 … −20 °C",6],[37064,2,"u","Beginn −20 … −10 °C",6],[37066,2,"u","Beginn −10 … 0 °C",6],[37068,2,"u","Beginn 0 … +10 °C",6],[37070,2,"u","Beginn +10 … +20 °C",6],[37072,2,"u","Beginn +20 … +30 °C",6],[37074,2,"u","Beginn +30 … +40 °C",6],[37076,2,"u","Beginn +40 … +50 °C",6],[37078,2,"u","Beginn +50 … +60 °C",6],[37080,2,"u","Beginn +60 … +70 °C",6],[37082,2,"u","Beginn +70 … +80 °C",6],[37084,2,"u","Beginn ab +80 °C",6],[37086,2,"u","Ende −30 … −20 °C",6],[37088,2,"u","Ende −20 … −10 °C",6],[37090,2,"u","Ende −10 … 0 °C",6],[37092,2,"u","Ende 0 … +10 °C",6],[37094,2,"u","Ende +10 … +20 °C",6],[37096,2,"u","Ende +20 … +30 °C",6],[37098,2,"u","Ende +30 … +40 °C",6],[37100,2,"u","Ende +40 … +50 °C",6],[37102,2,"u","Ende +50 … +60 °C",6],[37104,2,"u","Ende +60 … +70 °C",6],[37106,2,"u","Ende +70 … +80 °C",6],[37108,2,"u","Ende ab +80 °C",6],[37110,2,"u","0 min – 15 min",7],[37112,2,"u","15 min – 29 min",7],[37114,2,"u","29 min – 44 min",7],[37116,2,"u","44 min – 58 min",7],[37118,2,"u","58 min – 1:13 h",7],[37120,2,"u","1:13 h – 1:27 h",7],[37122,2,"u","1:27 h – 1:42 h",7],[37124,2,"u","1:42 h – 1:57 h",7],[37126,2,"u","1:57 h – 2:11 h",7],[37128,2,"u","2:11 h – 2:26 h",7],[37130,2,"u","2:26 h – 2:40 h",7],[37132,2,"u","2:40 h – 2:55 h",7],[37134,2,"u","2:55 h – 3:09 h",7],[37136,2,"u","3:09 h – 3:24 h",7],[37138,2,"u","0 min – 17 min",8],[37140,2,"u","17 min – 34 min",8],[37142,2,"u","34 min – 51 min",8],[37144,2,"u","51 min – 1:08 h",8],[37146,2,"u","1:08 h – 1:25 h",8],[37148,2,"u","1:25 h – 1:42 h",8],[37150,2,"u","1:42 h – 1:59 h",8],[37152,2,"u","1:59 h – 2:17 h",8],[37154,2,"u","2:17 h – 2:34 h",8],[37156,2,"u","2:34 h – 2:51 h",8],[37158,2,"u","2:51 h – 3:08 h",8],[37160,2,"u","3:08 h – 3:25 h",8],[37162,2,"u","3:25 h – 3:42 h",8],[37164,2,"u","3:42 h – 3:59 h",8],[37166,2,"u","Abgeschlossene Ladung (?)",9],[37168,2,"u","Unbekannt",9],[37170,2,"u","Unbekannt",9],[37172,2,"u","Unbekannt",9],[37174,2,"u","Unbekannt",9],[37176,2,"u","Unbekannt",9],[37178,2,"u","Unbekannt",9],[37180,2,"u","Unbekannt",9],[37182,2,"u","Unbekannt",9],[37184,2,"u","Unbekannt",9],[37186,2,"u","Unbekanntes Histogramm (Temperatur?)",9],[37188,2,"u","Unbekanntes Histogramm",9],[37190,2,"u","Unbekanntes Histogramm",9],[37192,2,"u","Unbekanntes Histogramm",9],[37194,2,"u","Unbekanntes Histogramm",9],[37196,2,"u","Unbekanntes Histogramm",9],[37198,2,"u","Unbekanntes Histogramm",9],[37200,2,"u","Unbekanntes Histogramm",9],[40960,6,"x","Unbekannt (Block 0xA000, Rohdaten)",9]];
/*REGS-END*/
/* every register the battery delivered, decoded from the raw blocks ("mem") like m18.py read_id() */
const dur=s=>s==null?"–":s<60?s+" s":s<3600?Math.floor(s/60)+" min "+String(s%60).padStart(2,"0")+" s":
  Math.floor(s/3600)+" h "+String(Math.floor(s%3600/60)).padStart(2,"0")+" min";
const hex4=a=>"0x"+a.toString(16).toUpperCase().padStart(4,"0");
function regBytes(mem){const b={};for(const k in mem||{}){const a=parseInt(k,16),h=mem[k];
  for(let i=0;i<h.length/2;i++)b[a+i]=parseInt(h.substr(2*i,2),16);}return b;}
function regVal(b,r){const a=r[0],n=r[1],t=r[2],x=[];
  for(let i=0;i<n;i++){if(b[a+i]===undefined)return null;x.push(b[a+i]);}
  let u=0;for(const v of x)u=u*256+v;
  switch(t){
    case"u":return{v:u,s:nf(u)};
    case"d":return{v:u,s:u>1104537600?new Date(u*1000).toLocaleString("de-DE",{dateStyle:"medium",timeStyle:"short",timeZone:"UTC"})+" UTC":"–"};
    case"t":return{v:u,s:dur(u)};
    case"a":{const t=x.map(c=>c>=32&&c<127?String.fromCharCode(c):" ").join("").replace(/[- ]+$/,"");return{v:t,s:t||"–"};}
    case"s":{const ty=x[0]*256+x[1],sn=x[2]*65536+x[3]*256+x[4];return{v:ty+"-"+sn,s:"Typ "+ty+" · Nr. "+sn};}
    case"T":{if(!u)return{v:0,s:"–"};const r=1e4+(u-0x180)*1e4/(0x22E-0x180),m=-15/1e4,c=m*r+(50-m*1e4);return{v:+c.toFixed(1),s:nf(c,1)+" °C"};}
    case"F":{if(!u)return{v:0,s:"–"};const c=x[0]+x[1]/256;return{v:+c.toFixed(2),s:nf(c,1)+" °C"};}
    case"c":{const c=[0,1,2,3,4].map(i=>x[2*i]*256+x[2*i+1]);return{v:c.join("/"),s:c.map(v=>nf(v/1000,3)).join(" · ")+" V"};}
    default:{const h=x.map(v=>v.toString(16).padStart(2,"0")).join("").toUpperCase();return{v:h,s:h||"–"};}}}
const regStore={};let regKey=0;
function allRegs(d){if(!d||!d.mem)return"";const b=regBytes(d.mem);let got=0;
  const html=REG_GROUPS.map((g,gi)=>{const name=g[0],hist=g[1],unit=g[2];
    const rows=REGS.filter(r=>r[4]==gi).map(r=>[r,regVal(b,r)]);
    const nums=rows.map(x=>x[1]&&typeof x[1].v=="number"?x[1].v:0),max=Math.max(1,...nums),sum=nums.reduce((a,c)=>a+c,0);
    const body=rows.map(([r,v])=>{if(v)got++;
      const txt=!v?'<span class="faint">nicht verfügbar</span>':esc(unit=="s"&&typeof v.v=="number"?dur(v.v):v.s);
      const bar=hist?`<span class="track"><span class="fill" style="width:${v&&v.v?Math.max(2,v.v/max*100):0}%"></span></span>`:"";
      return `<div class="reg${hist?" h":""}"><span class="lab">${esc(r[3])}<small>${hex4(r[0])}</small></span>${bar}<span class="rv">${txt}</span></div>`;}).join("");
    return `<details class="rg"${gi<3?" open":""}><summary>${esc(name)}${hist?` <span class="hint">· gesamt ${unit=="s"?dur(sum):nf(sum)}</span>`:""}</summary>${body}</details>`;}).join("");
  const k="r"+(++regKey);regStore[k]=d;
  return `<div class="card"><h2>Alle Werte aus dem Akku · ${got} von ${REGS.length}</h2>${html}
    <p class="hint">„nicht verfügbar“ = dieser Akku liefert den Wert nicht (z. B. Forge-Register bei älteren Akkus).</p>
    <div style="display:flex;gap:8px;margin-top:10px"><button class="btn" onclick="regCsv('${k}')">Alle Werte als CSV</button>
    <button class="btn" onclick="regJson('${k}')">Rohdaten (JSON)</button></div></div>`;}
function regSave(name,text,type){const a=document.createElement("a");a.href=URL.createObjectURL(new Blob([text],{type}));
  a.download=name;document.body.appendChild(a);a.click();setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove();},500);}
function regCsv(k){const d=regStore[k];if(!d)return;const b=regBytes(d.mem),q=v=>'"'+String(v).replace(/"/g,'""')+'"';
  const lines=[["Gruppe","Register","Bezeichnung","Wert","Rohwert"].join(";")];
  for(const r of REGS){const v=regVal(b,r),raw=[];for(let i=0;i<r[1];i++)if(b[r[0]+i]!==undefined)raw.push(b[r[0]+i].toString(16).padStart(2,"0"));
    lines.push([q(REG_GROUPS[r[4]][0]),hex4(r[0]),q(r[3]),q(v?(r[2]=="u"||r[2]=="t"?v.v:v.s):""),raw.join("").toUpperCase()].join(";"));}
  regSave("m18-"+d.eserial+"-alle-werte.csv","\ufeff"+lines.join("\r\n"),"text/csv");}
function regJson(k){const d=regStore[k];if(d)regSave("m18-"+d.eserial+".json",JSON.stringify(d,null,1),"application/json");}
/* result view (summary, cells, all values, trace) */
function gauge(v,st){const r=48,c=2*Math.PI*r,f=v==null||v<0?0:Math.min(100,v)/100;
  return `<svg class="gauge" viewBox="0 0 120 120"><circle cx="60" cy="60" r="${r}" fill="none" stroke-width="10" class="bg"/>
  <circle cx="60" cy="60" r="${r}" fill="none" stroke-width="10" stroke-linecap="round" class="${st}" style="stroke:currentColor"
  stroke-dasharray="${c*f} ${c}" transform="rotate(-90 60 60)"/><text x="60" y="66" text-anchor="middle" font-size="28" font-weight="650" fill="currentColor">${v==null||v<0?"–":v}</text>
  <text x="60" y="84" text-anchor="middle" font-size="10" fill="#959ba3">von 100</text></svg>`;}
const STL={good:"Gut",watch:"Beobachten",weak:"Schwach",unknown:"Unbekannt"};
function hms(s){if(s==null)return"–";const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h+" h "+String(m).padStart(2,"0")+" min";}
function traceBox(d,open){const t=d&&d.trace||[];if(!t.length)return"";
  return `<details class="card"${open?" open":""}><summary style="cursor:pointer;font-weight:600">Ablauf beim Auslesen (${t.length} Schritte)</summary>
    <pre style="margin-top:10px">${esc(t.join("\n"))}</pre></details>`;}
function render(d,when){
  if(!d){$("#result").innerHTML="";return;}
  if(!d.ok){$("#result").innerHTML=`<div class="card"><div class="box err"><b>Auslesen fehlgeschlagen:</b> ${esc(d.error||"unbekannt")}<br>
    <span class="hint">Akku richtig aufgesteckt? Verkabelung J1/J2/B− prüfen. (${d.blocks_ok||0}/${d.blocks_total||0} Blöcke${d.attempts>1?", 2 Versuche":""}${d.first_error&&d.first_error!=d.error?", 1. Versuch: "+esc(d.first_error):""})</span></div></div>`+traceBox(d,true);return;}
  const c=d.counters||{},cells=d.cells_mv||[],lo=Math.min(...cells),hi=Math.max(...cells);
  const warns=(d.warnings||[]).length?`<div class="card"><div class="box err" style="margin:0">${d.warnings.map(w=>"⚠ "+esc(w)).join("<br>")}</div></div>`:"";
  const retryNote=d.attempts>1?`<div class="card"><div class="box info" style="margin:0">Erst im 2. Versuch gelesen. 1. Versuch: ${esc(d.first_error||"?")}</div></div>`:"";
  $("#result").innerHTML=warns+retryNote+`
  <div class="card"><div class="hero">${gauge(d.health,d.status)}
    <div class="title"><b>${esc(d.model)}</b><span class="mono">E-Seriennr. ${esc(d.eserial)}</span>${d.note?`<br><span>Notiz: „${esc(d.note)}“</span>`:""}
    <br><span class="badge ${d.status}">${STL[d.status]||d.status}</span> <span class="hint">Zustand (Schätzung)</span></div></div>
    <div class="stats">
      <div class="stat"><div class="k">Pack</div><div class="v">${nf(d.pack_v,2)}<small> V</small></div></div>
      <div class="stat"><div class="k">Temperatur</div><div class="v">${nf(d.temp_c,1)}<small> °C</small></div></div>
      <div class="stat"><div class="k">Imbalance</div><div class="v ${d.imbalance_mv>=100?"crit":d.imbalance_mv>=50?"watch":""}">${nf(d.imbalance_mv)}<small> mV</small></div></div>
      <div class="stat"><div class="k">Vollzyklen</div><div class="v">${nf(d.cycles)}</div></div>
      <div class="stat"><div class="k">Entladen</div><div class="v">${nf(d.discharge_ah)}<small> Ah</small></div></div>
      <div class="stat"><div class="k">Ladungen</div><div class="v">${nf(c.total_charges)}</div></div>
      <div class="stat"><div class="k">Überhitzung</div><div class="v ${c.overheat?"watch":""}">${nf(c.overheat)}</div></div>
      <div class="stat"><div class="k">Unterspannung</div><div class="v">${nf(c.lowvolt)}</div></div>
      <div class="stat"><div class="k">Ladung Zelle &lt; 2,5 V</div><div class="v ${c.lowv_charges?"crit":""}">${nf(c.lowv_charges)}</div></div>
    </div></div>
  <div class="two">
    <div class="card"><h2>Zellspannungen</h2>${cells.map((v,i)=>`<div class="cell"><span class="lab">Zelle ${i+1}</span>
      <span class="track"><span class="fill" style="width:${Math.max(4,Math.min(100,(v/1000-3)/1.2*100))}%"></span></span>
      <span class="cv ${v==lo||v==hi?"":""}">${nf(v/1000,3)} V</span></div>`).join("")}</div>
    <div class="card"><h2>Abzüge vom Zustand</h2>${(d.health_parts||[]).map(p=>`<div class="row part"><span>${esc(p.label)}<small>${esc(p.detail)}</small></span><span class="p">${p.penalty>0?"−"+nf(p.penalty,1):"0"}</span></div>`).join("")}</div>
  </div>
  <div class="two">
    <div class="card"><h2>Daten</h2>
      <div class="row"><span>Hergestellt</span><span>${fd(d.manufactured)}</span></div>
      <div class="row"><span>Erste Ladung</span><span>${fd(d.first_charge)}</span></div>
      <div class="row"><span>Letzte Ladung</span><span>${fd(d.last_charge)}</span></div>
      <div class="row"><span>Letzte Werkzeugnutzung</span><span>${fd(d.last_tool_use)}</span></div>
      <div class="row"><span>Tage seit Erstladung</span><span>${nf(c.days)}</span></div></div>
    <div class="card"><h2>Nutzung</h2>
      <div class="row"><span>Redlink / einfache Ladungen</span><span>${nf(c.redlink_charges)} / ${nf(c.dumb_charges)}</span></div>
      <div class="row"><span>Ladezeit gesamt</span><span>${hms(c.charge_time_s)}</span></div>
      <div class="row"><span>Zeit am Werkzeug (&gt; 10 A)</span><span>${hms(c.tool_time_s)}</span></div>
      <div class="row"><span>Entladen (Energie)</span><span>${nf(d.discharge_wh)} Wh</span></div>
      <div class="row"><span>Leer gefahren / Überstrom</span><span>${nf(c.discharge_to_empty)} / ${nf(c.overcurrent)}</span></div></div>
  </div>
  ${allRegs(d)}${traceBox(d,false)}
  <p class="hint">${when?"Ausgelesen "+when+" · ":""}${d.blocks_ok}/${d.blocks_total} Blöcke in ${nf(d.duration_ms/1000,1)} s${d.retries?` · ${d.retries} Wiederholungen`:""}</p>`;}
let INFO={};
async function logLoad(){const el=$("#logbox");if(!el)return;try{const d=await j("/api/log?n=250");
  let l=d.lines||[];if($("#logwarn").checked)l=l.filter(x=>/WARNUNG|FEHLER/.test(x));
  el.textContent=l.length?l.join("\n"):"(keine Einträge)";el.scrollTop=el.scrollHeight;}catch(e){el.textContent="Protokoll nicht abrufbar";}}
async function j(url,opt){const r=await fetch(url,opt);return r.json();}
function tab(name){$$("nav button").forEach(b=>b.classList.toggle("on",b.dataset.tab==name));$$("main>section").forEach(s=>s.hidden=s.id!="tab-"+name);
  if(name=="wlan")wifiState();if(name=="geraet"){devInfo();logLoad();cloudLinkLoad();}if(name=="cloud")cloudView();if(name=="verlauf"){histStats();histLoad(true);}}
$$("nav button").forEach(b=>b.onclick=()=>tab(b.dataset.tab));

async function info(){try{INFO=await j("/api/info");}catch(e){return;}
  const w=INFO.wifi,p=$("#pill");
  p.className="pill "+(w=="sta"?"ok":"ap");
  p.textContent=w=="sta"?"WLAN: "+INFO.ssid:"Einrichtungsmodus";
  $("#hostline").textContent=(w=="sta"?"http://"+INFO.host+" · "+INFO.ip:"WLAN „"+INFO.ap+"“ · "+INFO.ap_ip);
  if(!$("#host").value){$("#host").value=INFO.host.replace(".local","");hostPreview();}
  try{const hs=await j("/api/history/stats");$("#newcount").hidden=!hs.unsynced;$("#newcount").textContent=hs.unsynced||"";}catch(e){}
  const allowed=INFO.setup_client||INFO.unlocked;
  const lock=allowed?(INFO.unlocked?`<div class="box ok">Einstellungen entsperrt (noch ${INFO.unlock_s} s).</div>`:""):
    `<div class="box info">🔒 Änderungen sind gesperrt. Kurz die <b>BOOT-Taste</b> am Gerät drücken – dann 2 Minuten entsperrt (LED türkis).</div>`;
  $("#lockbox").innerHTML=lock;$("#otalock").innerHTML=lock;
  $$("#wifiform input,#wifiform button,#forget,#fwbtn,#fwfile,#newpass,#newpass2,#passbtn").forEach(e=>e.disabled=!allowed);
  $("#passstate").innerHTML=(INFO.auth?'<div class="box ok" style="margin-top:0">🔒 Passwortschutz ist aktiv.</div>':'<div class="box info" style="margin-top:0">Kein Passwort gesetzt – jeder im WLAN kann die Brücke nutzen.</div>')+lock;
  return INFO;}
function hostPreview(){$("#hostpreview").textContent="http://"+($("#host").value||"m18-bridge")+".local";}
$("#host").oninput=hostPreview;

const sleep=ms=>new Promise(r=>setTimeout(r,ms));
$("#readbtn").onclick=async()=>{const b=$("#readbtn"),pg=$("#prog");b.disabled=true;b.innerHTML='<span class="spin"></span> Lese Akku …';
  pg.hidden=false;pg.firstChild.style.width="0%";
  try{try{localStorage.setItem("m18.loc",$("#loc").value);localStorage.setItem("m18.comment",$("#comment").value);}catch(e){}
    const body=new URLSearchParams({t:Math.floor(Date.now()/1000),loc:$("#loc").value,comment:$("#comment").value});
    const r=await fetch("/api/read",{method:"POST",body});
    if(r.status==409){$("#readhint").textContent="Gerät ist beschäftigt (PC liest gerade über die Brücke) – gleich nochmal versuchen.";return;}
    for(let i=0;i<120;i++){await sleep(500);let st;try{st=await j("/api/status");}catch(e){continue;}
      pg.firstChild.style.width=st.progress+"%";if(!st.reading)break;}
    const d=await j("/api/last");render(d,new Date().toLocaleTimeString("de-DE"));
    cloudAfterRead(d,{ts:Math.floor(Date.now()/1000),ts_src:window.M18_BLE?"app":"browser",loc:$("#loc").value,comment:$("#comment").value});
    if(d.history_id)$("#readhint").textContent="Gespeichert als Messung #"+d.history_id+" – wird bei der nächsten Übernahme in die PC-Datenbank übertragen.";
    $("#comment").value="";info();}
  catch(e){$("#result").innerHTML=`<div class="card"><div class="box err">Keine Antwort vom Gerät.</div></div>`;}
  finally{b.disabled=false;b.textContent="Akku erneut auslesen";pg.hidden=true;}};

/* ---------- WLAN ---------- */
async function wifiState(){const s=await j("/api/wifi");let h="";
  if(s.state=="connected"||s.state=="sta")h=`<div class="box ok">Verbunden mit <b>${esc(s.ssid)}</b> (${s.rssi} dBm)<br>
    Adresse: <span class="big-url">http://${esc(s.host)}</span><br><span class="mono">IP ${esc(s.ip)}</span></div>`;
  else if(s.state=="connecting")h=`<div class="box info"><span class="spin" style="display:inline-block;vertical-align:middle"></span> Verbinde mit ${esc(s.ssid)} …</div>`;
  else h=`<div class="box info">Einrichtungsmodus: Dieses Gerät hat ein eigenes WLAN <b>${esc(s.ap)}</b> geöffnet.
    ${s.ssid?`<br>Gespeichertes WLAN „${esc(s.ssid)}“ ist nicht erreichbar.`:"<br>Noch kein WLAN eingerichtet."}</div>`;
  if(s.error)h+=`<div class="box err">${esc(s.error)}</div>`;
  $("#wifistate").innerHTML=h;return s;}
$("#scanbtn").onclick=async()=>{const b=$("#scanbtn");b.disabled=true;b.innerHTML='<span class="spin"></span> Suche …';
  try{let r={scanning:true};for(let i=0;i<30&&r.scanning;i++){r=await j("/api/networks");if(r.scanning)await sleep(700);}
    const l=r.networks||[];$("#nets").innerHTML=l.length?l.map(n=>`<div class="net" data-ssid="${esc(n.ssid)}"><span>${n.secure?"🔒 ":""}${esc(n.ssid)}</span>
    <span class="bars">${n.rssi>-60?"▂▄▆█":n.rssi>-70?"▂▄▆":n.rssi>-80?"▂▄":"▂"} ${n.rssi} dBm</span></div>`).join(""):'<p class="hint">Keine Netzwerke gefunden.</p>';
    $$(".net").forEach(n=>n.onclick=()=>{$("#ssid").value=n.dataset.ssid;$("#pass").focus();});}
  finally{b.disabled=false;b.textContent="Erneut suchen";}};
$("#wifiform").onsubmit=async e=>{e.preventDefault();const body=new URLSearchParams(new FormData(e.target));
  $("#wifiprogress").innerHTML=`<div class="box info"><span class="spin" style="display:inline-block;vertical-align:middle"></span> Verbinde mit ${esc($("#ssid").value)} … (bis zu 20 s)</div>`;
  const resp=await fetch("/api/wifi",{method:"POST",body});
  if(!resp.ok){const e=await resp.json().catch(()=>({}));$("#wifiprogress").innerHTML=`<div class="box err">${esc(e.error||"Fehler")}</div>`;return;}
  const host=($("#host").value||"m18-bridge")+".local";
  for(let i=0;i<30;i++){await new Promise(r=>setTimeout(r,1000));let s;
    try{s=await j("/api/wifi");}catch(err){continue;}
    if(s.state=="connected"||s.state=="sta"){$("#wifiprogress").innerHTML=`<div class="box ok"><b>✓ Verbunden!</b><br>
      Das Gerät ist jetzt im WLAN „${esc(s.ssid)}“ erreichbar unter<br><span class="big-url">http://${esc(s.host)}</span><br>
      <span class="mono">IP-Adresse: ${esc(s.ip)}</span><br><br>Verbinde dein Handy/Laptop wieder mit deinem normalen WLAN.
      Der Einrichtungs-Hotspot schließt sich in ${s.ap_closes_in||90} Sekunden. Die PC-Software findet die Brücke über „Suchen“.</div>`;info();wifiState();return;}
    if(s.state=="failed"){$("#wifiprogress").innerHTML=`<div class="box err">Verbindung fehlgeschlagen: ${esc(s.error||"")}. Passwort prüfen und erneut versuchen.</div>`;return;}}
  $("#wifiprogress").innerHTML=`<div class="box info">Keine Rückmeldung mehr – vermutlich hat das Gerät den Funkkanal gewechselt.
    Verbinde dich wieder mit deinem WLAN und öffne <span class="big-url">http://${esc(host)}</span>.</div>`;};
$("#forget").onclick=async()=>{if(!confirm("WLAN-Daten löschen und neu starten?"))return;const r=await fetch("/api/wifi/forget",{method:"POST"});
  if(!r.ok){const e=await r.json().catch(()=>({}));$("#wifistate").innerHTML=`<div class="box err">${esc(e.error||"Fehler")}</div>`;return;}
  $("#wifistate").innerHTML='<div class="box info">Gelöscht – das Gerät startet im Einrichtungsmodus neu.</div>';};

/* ---------- Gerät ---------- */
async function devInfo(){const i=await info();if(!i)return;ledInit();sleepInit();
  $("#devinfo").innerHTML=[["Firmware",i.version],["Gerätename",i.host],["IP",i.ip||"–"],["WLAN",i.wifi=="sta"?i.ssid+" ("+i.rssi+" dBm)":"Einrichtungsmodus ("+i.ap+")"],
   ["MAC",i.mac],["Akku-Leitung",i.line=="idle"?"Ruhe (J2 low)":"aktiv"],["Laufzeit",hms(i.uptime_s)],["Letzter Neustart",i.reset_reason],
   ["Stromversorgung",i.brownouts?"⚠ "+i.brownouts+"× Unterspannung seit dem Einschalten"+(i.tx_low?", WLAN-Sendeleistung reduziert":"")+" – USB-Kabel/-Anschluss bzw. Step-down prüfen":"in Ordnung"],
   ["Freier Speicher",nf(i.heap_kb)+" KB (min. "+nf(i.min_heap_kb)+") · PSRAM "+nf(i.psram_kb)+" KB"],
   ["Uhrzeit",i.time||"nicht synchronisiert"],["Build",i.build]].map(r=>`<div class="row"><span>${r[0]}</span><span class="mono">${esc(r[1])}</span></div>`).join("");}
async function cmd(text){return (await fetch("/cmd",{method:"POST",headers:{"Content-Type":"text/plain"},body:text})).text();}
$("#testbtn").onclick=async()=>{$("#testres").innerHTML='<p class="hint">…</p>';const r=await cmd("RESET\nIDLE");
  $("#testres").innerHTML=r.startsWith("OK ACK")?'<div class="box ok">✓ Akku antwortet (Sync 0xAA)</div>':`<div class="box err">✗ Keine Antwort – Verkabelung prüfen<br><span class="mono">${esc(r)}</span></div>`;};
/* ---------- Verlauf ---------- */
let hNext=0,hItems=[];
function fdt(ts){return ts?new Date(ts*1000).toLocaleString("de-DE",{dateStyle:"short",timeStyle:"short"}):"Zeit unbekannt";}
async function histStats(){const s=await j("/api/history/stats");
  if(s.ok===false||!s.ready){$("#hstats").innerHTML='<div class="box err" style="margin-top:0">Kein Speicher verfügbar – Partition Scheme „16M Flash (3MB APP/9.9MB FATFS)“ wählen.</div>';return s;}
  $("#hstats").innerHTML=`<div class="row"><span>Messungen</span><span>${nf(s.count)}</span></div>
    <div class="row"><span>Noch nicht in der PC-Datenbank</span><span class="${s.unsynced?"warn":""}">${nf(s.unsynced)}</span></div>
    <div class="row"><span>Speicher frei</span><span>${nf(s.free_kb/1024,1)} von ${nf(s.total_kb/1024,1)} MB</span></div>`;return s;}
function histRow(m){return `<div class="net" data-id="${m.id}"><span><b>${esc(m.note||m.eserial||"?")}</b> <span class="mono">${esc(m.model||"")}</span><br>
  <span class="hint" style="margin:0">${fdt(m.ts)}${m.loc?" · 📍 "+esc(m.loc):""}${m.comment?" · "+esc(m.comment):""}</span></span>
  <span style="text-align:right"><b class="${m.health>=80?"good":m.health>=60?"watch":"weak"}">${m.health??"–"}</b><br>
  <span class="bars">${m.synced?"✓ übernommen":"neu"}</span></span></div>`;}
async function histLoad(reset){
  if(reset){hNext=0;hItems=[];}
  // newest first: page through all ids once (ascending), then show reversed
  let after=hNext,guard=0;const page=[];
  do{const r=await j("/api/history?after="+after+"&max=100&filter=all");if(r.ok===false)break;page.push(...r.items);after=r.next;guard++;}while(after&&guard<40);
  hItems=page.reverse();
  $("#hlist").innerHTML=hItems.length?hItems.map(histRow).join(""):'<p class="hint">Noch keine Messungen gespeichert.</p>';
  $$("#hlist .net").forEach(n=>n.onclick=()=>histView(n.dataset.id));}
async function histView(id){const r=await j("/api/history/item?id="+id);if(r.ok===false)return;
  const m=r.meta;render(r.scan,fdt(m.ts));const res=$("#result").innerHTML;$("#result").innerHTML="";
  $("#hview").innerHTML=`<div class="card"><h2>Messung #${m.id} · ${fdt(m.ts)}${m.loc?" · "+esc(m.loc):""}</h2>${m.comment?`<p>${esc(m.comment)}</p>`:""}
    <button class="btn" id="hdel">Diese Messung löschen</button></div>`+res;
  $("#hdel").onclick=async()=>{if(!confirm("Messung #"+m.id+" löschen?"))return;await fetch("/api/history/delete",{method:"POST",body:new URLSearchParams({id:m.id})});
    $("#hview").innerHTML="";histStats();histLoad(true);};
  $("#hview").scrollIntoView({behavior:"smooth"});}
$("#hexport").onclick=async()=>{const b=$("#hexport");b.disabled=true;const recs=[];
  try{for(let i=0;i<hItems.length;i++){b.textContent="Exportiere "+(i+1)+"/"+hItems.length+" …";const r=await j("/api/history/item?id="+hItems[i].id);if(r.meta)recs.push(r);}
    const blob=new Blob([JSON.stringify({format:"m18-bridge-export",version:1,device:INFO.host,exported:new Date().toISOString(),records:recs})],{type:"application/json"});
    const a=document.createElement("a");a.href=URL.createObjectURL(blob);a.download="m18-bruecke-"+new Date().toISOString().slice(0,10)+".json";a.click();}
  finally{b.disabled=false;b.textContent="Alle als JSON exportieren";}};
$("#hclear").onclick=async()=>{if(!confirm("Alle bereits in die PC-Datenbank übernommenen Messungen von der Brücke löschen?"))return;
  const r=await j("/api/history/clear",{method:"POST"});alert(r.deleted+" Messungen gelöscht");histStats();histLoad(true);};
/* ---------- Passwort ---------- */
$("#passbtn").onclick=async()=>{const a=$("#newpass").value,b=$("#newpass2").value;
  if(a!==b){alert("Die Passwörter stimmen nicht überein");return;}
  const r=await fetch("/api/password",{method:"POST",body:new URLSearchParams({pass:a})});const e=await r.json().catch(()=>({}));
  if(!r.ok){alert("Fehler: "+(e.error||r.status));return;}
  alert(a?"Passwort gesetzt. Der Browser fragt gleich danach (Benutzer „m18“).":"Passwortschutz ausgeschaltet.");$("#newpass").value=$("#newpass2").value="";location.reload();};
/* Energiesparen */
function sleepInit(){if(INFO.sleep_min==null)return;$("#sleepmin").value=String(INFO.sleep_min);
  if(![...$("#sleepmin").options].some(o=>o.value==String(INFO.sleep_min))){const o=document.createElement("option");o.value=o.textContent=INFO.sleep_min;$("#sleepmin").appendChild(o);$("#sleepmin").value=String(INFO.sleep_min);}
  $("#sleepstate").innerHTML=INFO.sleep_min?`<p class="hint" style="margin:0 0 6px">Schläft in ca. ${Math.ceil(INFO.sleep_in_s/60)} min, wenn niemand die Brücke benutzt.</p>`:"";}
$("#sleepsave").onclick=async()=>{const r=await fetch("/api/power",{method:"POST",body:new URLSearchParams({sleep_min:$("#sleepmin").value})});
  alert(r.ok?"Gespeichert":"Fehler");devInfo();};
/* LED */
function ledPinValue(){const v=$("#ledpin").value;return v=="custom"?$("#ledcustom").value:v;}
function ledInit(){if(INFO.led_pin==null)return;const p=String(INFO.led_pin);
  if([...$("#ledpin").options].some(o=>o.value==p))$("#ledpin").value=p;else{$("#ledpin").value="custom";$("#ledcustom").hidden=false;$("#ledcustom").value=p;}
  $("#ledbright").value=INFO.led_brightness;}
$("#ledpin").onchange=()=>{$("#ledcustom").hidden=$("#ledpin").value!="custom";};
$("#ledtest").onclick=()=>fetch("/api/led/test",{method:"POST",body:new URLSearchParams({pin:ledPinValue()})});
$("#ledsave").onclick=async()=>{const r=await fetch("/api/led",{method:"POST",body:new URLSearchParams({pin:ledPinValue(),brightness:$("#ledbright").value})});
  const e=await r.json().catch(()=>({}));alert(r.ok?"LED-Einstellung gespeichert":"Fehler: "+(e.error||r.status));};
$("#ledbright").onchange=()=>fetch("/api/led",{method:"POST",body:new URLSearchParams({brightness:$("#ledbright").value})});
/* Firmware update */
$("#fwbtn").onclick=()=>{const f=$("#fwfile").files[0];if(!f){alert("Bitte zuerst die .bin-Datei wählen");return;}
  if(!confirm("Firmware "+f.name+" ("+Math.round(f.size/1024)+" KB) aufspielen? Das Gerät startet danach neu."))return;
  const x=new XMLHttpRequest(),fd=new FormData(),pg=$("#fwprog");fd.append("firmware",f,f.name);pg.hidden=false;
  x.upload.onprogress=e=>{if(e.lengthComputable)pg.firstChild.style.width=(e.loaded/e.total*100)+"%";};
  x.onload=()=>{let r={};try{r=JSON.parse(x.responseText);}catch(e){}
    $("#fwres").innerHTML=r.ok?'<div class="box ok">✓ Update erfolgreich – Gerät startet neu. Seite in ca. 15 s neu laden.</div>'
      :`<div class="box err">Update fehlgeschlagen: ${esc(r.error||x.status)}</div>`;};
  x.onerror=()=>{$("#fwres").innerHTML='<div class="box err">Verbindung abgebrochen.</div>';};
  x.open("POST","/api/update");x.send(fd);};
$("#cmdbtn").onclick=async()=>{const p=$("#cmdres");p.hidden=false;p.textContent="…";p.textContent=await cmd($("#cmd").value);};
$("#logref").onclick=logLoad;$("#logwarn").onchange=logLoad;
$("#logclr").onclick=async()=>{if(!confirm("Protokoll der Brücke löschen?"))return;const r=await fetch("/api/log/clear",{method:"POST"});
  if(!r.ok){const e=await r.json().catch(()=>({}));alert(e.error||"Fehler");}logLoad();};

try{$("#loc").value=localStorage.getItem("m18.loc")||"";}catch(e){}

/* ---------- Cloud (Supabase): Konto, Meine Akkus, Hochladen, Brücke verknüpfen ---------- */
const SB={url:"https://seccxaldsjhukavoilnm.supabase.co",key:"sb_publishable_KSiLpnnPHzgOvS3cTPnl_A_OVknKkj1",
  app:"https://danunt97.github.io/m18-protocol/app/"};
const ls={get(k){try{return JSON.parse(localStorage.getItem(k));}catch(e){return null;}},
  set(k,v){try{v==null?localStorage.removeItem(k):localStorage.setItem(k,JSON.stringify(v));}catch(e){}}};
let SESS=ls.get("m18.sb.sess"),ORG=ls.get("m18.sb.org"),CMSG="";
async function sbRaw(path,opt={}){const h={apikey:SB.key,"Content-Type":"application/json"};if(opt.token)h.Authorization="Bearer "+opt.token;
  let r;try{r=await fetch(SB.url+path,{method:opt.method||"GET",headers:h,body:opt.body?JSON.stringify(opt.body):undefined});}
  catch(e){throw new Error("Keine Internetverbindung");}
  const t=await r.text();let d=null;try{d=t?JSON.parse(t):null;}catch(e){d=t;}
  if(!r.ok)throw new Error(sbErr(d)||("HTTP "+r.status));return d;}
function sbErr(d){const m=d&&(d.msg||d.message||d.error_description||d.error);if(!m)return"";
  const de={"Invalid login credentials":"E-Mail oder Passwort falsch","Email not confirmed":"E-Mail-Adresse noch nicht bestätigt – bitte den Link in der Mail öffnen",
    "User already registered":"Für diese E-Mail gibt es schon ein Konto","email rate limit exceeded":"Zu viele E-Mails – bitte später nochmal versuchen"};
  for(const k in de)if(String(m).indexOf(k)>=0)return de[k];return String(m);}
function setSess(d){SESS=d?{at:d.access_token,rt:d.refresh_token,exp:Date.now()/1000+(+d.expires_in||3600)-60,
    email:(d.user&&d.user.email)||(SESS&&SESS.email)||""}:null;ls.set("m18.sb.sess",SESS);if(!d){ORG=null;ls.set("m18.sb.org",null);}}
async function sbToken(){if(!SESS)return null;if(Date.now()/1000<SESS.exp)return SESS.at;
  try{setSess(await sbRaw("/auth/v1/token?grant_type=refresh_token",{method:"POST",body:{refresh_token:SESS.rt}}));return SESS.at;}
  catch(e){if(e.message!="Keine Internetverbindung")setSess(null);throw e;}}
async function sb(path,opt={}){const token=await sbToken();if(!token)throw new Error("Nicht angemeldet");return sbRaw(path,{...opt,token});}
async function sbOrg(){if(ORG)return ORG;const o=await sb("/rest/v1/orgs?select=id,name&order=created_at&limit=1");ORG=o[0]||null;ls.set("m18.sb.org",ORG);return ORG;}
function cq(){return ls.get("m18.sb.queue")||[];}
function cloudBadge(){const n=cq().length,e=$("#qcount");if(e){e.hidden=!n;e.textContent=n?"↑"+n:"";}}
async function cloudAfterRead(d,meta){if(!d||!d.ok||!d.mem)return;const q=cq();q.push({scan:d,meta,src:window.M18_BLE?"app-ble":"app-wlan"});
  ls.set("m18.sb.queue",q.slice(-200));cloudBadge();if(SESS)cloudFlush();}
async function cloudFlush(){let q=cq(),sent=0;if(!SESS||!q.length){cloudBadge();return sent;}
  try{const org=await sbOrg();if(!org)throw new Error("keine Firma gefunden");
    while(q.length){const it=q[0];const r=await sb("/rest/v1/rpc/app_ingest",{method:"POST",body:{p_org:org.id,p_payload:{scan:it.scan,meta:it.meta},p_source:it.src}});
      if(!r||(!r.ok&&!/gültige/.test(r.error||"")))throw new Error((r&&r.error)||"Fehler");q.shift();ls.set("m18.sb.queue",q);sent++;}
    CMSG="";}catch(e){CMSG="Noch nicht hochgeladen: "+e.message+" – wird später erneut versucht.";}
  cloudBadge();if($("#tab-cloud")&&!$("#tab-cloud").hidden)cloudHead();return sent;}
function cloudInit(){
  const h=new URLSearchParams(location.hash.replace(/^#/,""));          /* link from a confirmation / password mail */
  if(h.get("access_token")){setSess({access_token:h.get("access_token"),refresh_token:h.get("refresh_token"),expires_in:h.get("expires_in")});
    history.replaceState(null,"",location.pathname+location.search);
    sb("/auth/v1/user").then(u=>{SESS.email=u.email;ls.set("m18.sb.sess",SESS);cloudHead();}).catch(()=>{});
    if(h.get("type")=="recovery")setTimeout(()=>{tab("cloud");cloudPwForm();},300);else setTimeout(()=>tab("cloud"),300);}
  else if(h.get("error_description"))CMSG=h.get("error_description");
  cloudBadge();setTimeout(cloudFlush,1500);setInterval(cloudFlush,120000);addEventListener("online",cloudFlush);}
function cbox(html){$("#cloudbox").innerHTML=html;}
function cloudHead(){if(!SESS){cloudLogin();return;}const n=cq().length;
  cbox(`<div class="card"><h2>Konto</h2><div class="row"><span>Angemeldet</span><span>${esc(SESS.email||"…")}</span></div>
    <div class="row"><span>Firma</span><span>${esc(ORG?ORG.name:"…")}</span></div>
    <div class="row"><span>Warten auf Upload</span><span>${n?n+" Messung"+(n>1?"en":""):"nichts – alles in der Cloud"}</span></div>
    ${CMSG?`<div class="box info">${esc(CMSG)}</div>`:""}
    <div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:12px">${n?'<button class="btn primary" id="cqsend">Jetzt hochladen</button>':""}
      <button class="btn" id="clogout">Abmelden</button><button class="btn" id="cdel">Konto löschen</button></div></div>`);
  if($("#cqsend"))$("#cqsend").onclick=async()=>{$("#cqsend").disabled=true;await cloudFlush();cloudView();};
  $("#clogout").onclick=async()=>{try{await sbRaw("/auth/v1/logout",{method:"POST",token:SESS.at});}catch(e){}setSess(null);cloudView();};
  $("#cdel").onclick=async()=>{if(!confirm("Konto und alle Daten deiner Firma in der Cloud endgültig löschen?"))return;
    if(!confirm("Wirklich? Das lässt sich nicht rückgängig machen."))return;
    try{await sb("/rest/v1/rpc/delete_my_account",{method:"POST",body:{}});setSess(null);ls.set("m18.sb.queue",null);cloudView();}
    catch(e){alert("Löschen fehlgeschlagen: "+e.message);}};
  if(!ORG)sbOrg().then(()=>cloudHead()).catch(()=>{});}
function cloudLogin(mode){mode=mode||"login";
  const f=mode=="signup"?`<label>Firma / Name</label><input id="cfirm" maxlength="100" placeholder="z. B. Elektro Huber">`:"";
  cbox(`<div class="card"><h2>${mode=="signup"?"Konto anlegen":mode=="reset"?"Passwort vergessen":"Anmelden"}</h2>
    <p class="hint" style="margin-top:0">Mit einem Konto landen alle Messungen in der Cloud – von jedem Handy, jeder Brücke und dem PC an einem Ort.
      Ohne Internet (z. B. im Einrichtungs-WLAN der Brücke) werden Messungen gemerkt und später hochgeladen.</p>
    ${f}<label>E-Mail</label><input id="cmail" type="email" autocomplete="email" value="${esc(ls.get("m18.sb.mail")||"")}">
    ${mode=="reset"?"":`<label>Passwort</label><input id="cpw" type="password" autocomplete="${mode=="signup"?"new-password":"current-password"}">`}
    ${mode=="signup"?'<label>Passwort wiederholen</label><input id="cpw2" type="password" autocomplete="new-password"><p class="hint">Mindestens 8 Zeichen.</p>':""}
    <button class="btn primary big" id="cgo" style="margin-top:14px">${mode=="signup"?"Konto anlegen":mode=="reset"?"Link zum Zurücksetzen senden":"Anmelden"}</button>
    <div id="cmsg">${CMSG?`<div class="box info">${esc(CMSG)}</div>`:""}</div>
    <p class="hint" style="margin-top:12px">${mode!="login"?'<a href="#" data-m="login" style="color:inherit">Anmelden</a> · ':""}${mode!="signup"?'<a href="#" data-m="signup" style="color:inherit">Neues Konto anlegen</a> · ':""}${mode!="reset"?'<a href="#" data-m="reset" style="color:inherit">Passwort vergessen</a>':""}</p></div>`);
  $$("#cloudbox [data-m]").forEach(a=>a.onclick=e=>{e.preventDefault();CMSG="";cloudLogin(a.dataset.m);});
  const msg=(t,ok)=>$("#cmsg").innerHTML=`<div class="box ${ok?"ok":"err"}">${esc(t)}</div>`;
  $("#cgo").onclick=async()=>{const mail=$("#cmail").value.trim();ls.set("m18.sb.mail",mail);if(!mail){msg("Bitte E-Mail eingeben");return;}
    const b=$("#cgo");b.disabled=true;
    try{if(mode=="login"){setSess(await sbRaw("/auth/v1/token?grant_type=password",{method:"POST",body:{email:mail,password:$("#cpw").value}}));
        CMSG="";await sbOrg().catch(()=>{});await cloudFlush();cloudView();return;}
      if(mode=="signup"){if($("#cpw").value.length<8){msg("Passwort: mindestens 8 Zeichen");return;}
        if($("#cpw").value!=$("#cpw2").value){msg("Passwörter stimmen nicht überein");return;}
        const r=await sbRaw("/auth/v1/signup?redirect_to="+encodeURIComponent(SB.app),{method:"POST",
          body:{email:mail,password:$("#cpw").value,data:{org_name:$("#cfirm").value.trim()}}});
        if(r&&r.access_token){setSess(r);cloudView();return;}
        CMSG="Fast fertig: Wir haben dir eine E-Mail an "+mail+" geschickt. Bitte den Link darin öffnen, danach hier anmelden.";cloudLogin("login");return;}
      await sbRaw("/auth/v1/recover?redirect_to="+encodeURIComponent(SB.app),{method:"POST",body:{email:mail}});
      msg("Wenn es ein Konto für diese Adresse gibt, kommt gleich eine E-Mail mit einem Link zum Zurücksetzen.",true);}
    catch(e){msg(e.message);}finally{b.disabled=false;}};}
function cloudPwForm(){cbox(`<div class="card"><h2>Neues Passwort</h2><label>Neues Passwort</label><input id="cnp" type="password" autocomplete="new-password">
    <button class="btn primary" id="cnpgo" style="margin-top:10px">Speichern</button><div id="cnpmsg"></div></div>`);
  $("#cnpgo").onclick=async()=>{if($("#cnp").value.length<8){$("#cnpmsg").innerHTML='<div class="box err">Mindestens 8 Zeichen</div>';return;}
    try{await sb("/auth/v1/user",{method:"PUT",body:{password:$("#cnp").value}});CMSG="Passwort geändert.";cloudView();}
    catch(e){$("#cnpmsg").innerHTML=`<div class="box err">${esc(e.message)}</div>`;}};}
const fdt2=s=>s?new Date(s).toLocaleString("de-DE",{dateStyle:"short",timeStyle:"short"}):"–";
const SRCN={"app-ble":"App (Bluetooth)","app-wlan":"App (WLAN)",bridge:"Brücke",pc:"PC"};
async function cloudView(){cloudHead();const v=$("#cloudview");if(!SESS){v.innerHTML="";return;}
  v.innerHTML='<div class="card"><h2>Meine Akkus</h2><p class="hint" style="margin-top:0">Lade …</p></div>';
  try{const bs=await sb("/rest/v1/battery_overview?select=*&order=last_measured_at.desc.nullslast");
    v.innerHTML=`<div class="card"><h2>Meine Akkus · ${bs.length}</h2>${bs.length?`<div class="nets">${bs.map(b=>`<div class="net" data-b="${b.id}">
      <span><b>${esc(b.name||b.eserial)}</b> ${esc(b.model||"")}<br><span class="hint">${b.name?esc(b.eserial)+" · ":""}${fdt2(b.last_measured_at)}${b.location?" · "+esc(b.location):""} · ${b.readings} Messung${b.readings==1?"":"en"}</span></span>
      <span class="badge ${b.last_status||"unknown"}">${b.last_health!=null?b.last_health:"–"}</span></div>`).join("")}</div>`
      :'<p class="hint">Noch keine Akkus in der Cloud. Nach dem nächsten Auslesen erscheinen sie hier.</p>'}</div><div id="cbat"></div>`;
    $$("#cloudview [data-b]").forEach(n=>n.onclick=()=>cloudBattery(n.dataset.b,bs.find(x=>x.id==n.dataset.b)));}
  catch(e){v.innerHTML=`<div class="card"><div class="box err">${esc(e.message)}</div></div>`;}}
async function cloudBattery(id,b){const el=$("#cbat");el.innerHTML='<p class="hint">Lade …</p>';
  try{const rs=await sb(`/rest/v1/readings?battery_id=eq.${id}&select=id,measured_at,health,status,source,location,comment,pack_v&order=measured_at.desc&limit=100`);
    el.innerHTML=`<div class="card"><h2>${esc(b.name||b.eserial)} · Messverlauf</h2><div class="nets">${rs.map(r=>`<div class="net" data-r="${r.id}">
      <span>${fdt2(r.measured_at)} <span class="hint">· ${esc(SRCN[r.source]||r.source)}${r.location?" · "+esc(r.location):""}</span>${r.comment?`<br><span class="hint">${esc(r.comment)}</span>`:""}</span>
      <span class="badge ${r.status||"unknown"}">${r.health!=null?r.health:"–"}</span></div>`).join("")}</div></div><div id="cread"></div>`;
    $$("#cbat [data-r]").forEach(n=>n.onclick=()=>cloudReading(n.dataset.r));el.scrollIntoView({behavior:"smooth"});}
  catch(e){el.innerHTML=`<div class="box err">${esc(e.message)}</div>`;}}
async function cloudReading(id){const el=$("#cread");
  try{const r=(await sb(`/rest/v1/readings?id=eq.${id}&select=scan,measured_at,location,comment,source`))[0];
    const prev=$("#result").innerHTML;render(r.scan,fdt2(r.measured_at));const html=$("#result").innerHTML;$("#result").innerHTML=prev;
    el.innerHTML=`<div class="card"><h2>Messung ${fdt2(r.measured_at)}${r.location?" · "+esc(r.location):""}</h2>${r.comment?`<p>${esc(r.comment)}</p>`:""}
      <p class="hint" style="margin-top:0">Quelle: ${esc(SRCN[r.source]||r.source)}</p></div>`+html;el.scrollIntoView({behavior:"smooth"});}
  catch(e){el.innerHTML=`<div class="box err">${esc(e.message)}</div>`;}}
/* Link this bridge to the account: the bridge then uploads its stored read-outs itself when it has internet. */
async function cloudLinkLoad(){const el=$("#cloudlink");if(!el)return;let st;
  try{st=await j("/api/cloud");}catch(e){el.innerHTML='<p class="hint" style="margin-top:0">Firmware zu alt für die Cloud (ab 1.12).</p>';return;}
  const last=st.last_ok?new Date(st.last_ok*1000).toLocaleString("de-DE",{dateStyle:"short",timeStyle:"short"}):"noch nie";
  el.innerHTML=st.configured?`<div class="row"><span>Verknüpft</span><span>${esc(st.host)}</span></div>
      <div class="row"><span>Warten auf Upload</span><span>${st.pending}</span></div><div class="row"><span>Zuletzt hochgeladen</span><span>${last}</span></div>
      <div class="row"><span>Internet</span><span>${st.online?"ja (WLAN)":"nein – lädt hoch, sobald die Brücke im WLAN mit Internet ist"}</span></div>
      ${st.error?`<div class="box err">${esc(st.error)}</div>`:""}
      <div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:10px"><button class="btn" id="clsync">Jetzt hochladen</button><button class="btn" id="cloff">Verknüpfung lösen</button></div><div id="clmsg"></div>`
    :`<p class="hint" style="margin-top:0">Verknüpfte Brücken laden ihre Messungen selbst in die Cloud, sobald sie im WLAN Internet haben – auch ohne Handy.</p>
      ${SESS?'<button class="btn primary" id="cllink">Mit meinem Konto verknüpfen</button>':'<p class="hint">Dazu zuerst im Tab <b>Cloud</b> anmelden.</p>'}<div id="clmsg"></div>`;
  const msg=(t,ok)=>$("#clmsg").innerHTML=`<div class="box ${ok?"ok":"err"}">${esc(t)}</div>`;
  const post=async(body)=>{const r=await fetch("/api/cloud",{method:"POST",body:new URLSearchParams(body)});const d=await r.json().catch(()=>({}));
    if(!r.ok)throw new Error(d.error||("Fehler "+r.status));return d;};
  if($("#clsync"))$("#clsync").onclick=async()=>{try{await post({action:"sync"});msg("Hochladen angestoßen.",true);setTimeout(cloudLinkLoad,4000);}catch(e){msg(e.message);}};
  if($("#cloff"))$("#cloff").onclick=async()=>{if(!confirm("Verknüpfung dieser Brücke lösen?"))return;try{await post({action:"off"});cloudLinkLoad();}catch(e){msg(e.message);}};
  if($("#cllink"))$("#cllink").onclick=async()=>{$("#cllink").disabled=true;
    try{const org=await sbOrg();const i=await info()||INFO;
      const r=await sb("/rest/v1/rpc/bridge_register",{method:"POST",body:{p_org:org.id,p_name:i.ble||i.ap||i.host,p_mac:i.mac}});
      if(!r.ok)throw new Error(r.error);
      await post({action:"link",url:SB.url,key:SB.key,token:r.token});msg("Verknüpft – die Brücke lädt ab jetzt selbst hoch.",true);setTimeout(cloudLinkLoad,1500);}
    catch(e){msg(e.message);}finally{if($("#cllink"))$("#cllink").disabled=false;}};}
let started=false;
async function start(){await info();try{const d=await j("/api/last");if(d&&d.eserial!==undefined)render(d,"zuletzt");}catch(e){}
  if(!window.M18_BLE&&INFO.wifi!="sta")tab("wlan");
  if(!started){started=true;setInterval(()=>{const t=$("nav button.on").dataset.tab;info();if(t=="wlan")wifiState();},3000);}}
cloudInit();
window.M18_START=start;                 /* the Bluetooth app (GitHub Pages) calls start() once connected */
if(!window.M18_BLE)start();
</script></body></html>)HTML";
