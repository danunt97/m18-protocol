#!/bin/bash
# Platine komplett neu aufbauen: Netze/Felder aus dem Schaltplan, Bauteillage (lage.py),
# Akkuleitungen vorrouten, Freerouting, GND-Flaechen, Beschriftung, DRC, Exporte.
# KiCad (Platine/Schaltplan) vorher schliessen. Aufruf aus hardware/platine: tools/neu_routen.sh
set -e
cd "$(dirname "$0")/.."
KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
CLI=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
JAVA=$(ls -d ~/.kicad-mcp/jre25/*/Contents/Home/bin/java | head -1)   # Freerouting 2.2.4 braucht Java 25
JAR=~/.kicad-mcp/freerouting.jar
TMP=$(mktemp -d)
k() { "$KPY" "$@" 2>&1 | grep -v -E "wxApp|memory leak|Debug: Adding" || true; }
python3 tools/abgleich.py
k tools/platzieren.py
k tools/vorrouten.py
k - <<PY
import pcbnew
b=pcbnew.LoadBoard('m18-bruecke.kicad_pcb'); print('DSN', pcbnew.ExportSpecctraDSN(b,'$TMP/m18.dsn'))
PY
# mehrere Laeufe (Ergebnis schwankt), der erste ohne offene Verbindungen gewinnt
best=999
for mp in 100 150 200 120 250 180; do
  "$JAVA" -Djava.awt.headless=true -jar "$JAR" -de "$TMP/m18.dsn" -do "$TMP/try.ses" -mp $mp --gui.enabled=false > "$TMP/fr.log" 2>&1 || true
  n=$(grep -oE 'final score: [0-9.]+ \(([0-9]+) unrouted' "$TMP/fr.log" | grep -oE '\(([0-9]+)' | tr -d '(' | tail -1)
  n=${n:-0}; grep -q "final score" "$TMP/fr.log" || n=999
  echo "Freerouting -mp $mp: $n offen"
  if [ "$n" -lt "$best" ]; then best=$n; cp "$TMP/try.ses" "$TMP/m18.ses"; fi
  [ "$best" -eq 0 ] && break
done
k - <<PY
import pcbnew
b=pcbnew.LoadBoard('m18-bruecke.kicad_pcb'); print('SES', pcbnew.ImportSpecctraSES(b,'$TMP/m18.ses')); b.Save('m18-bruecke.kicad_pcb')
PY
k tools/flaechen.py
k tools/beschriftung.py
python3 tools/abgleich.py
"$CLI" sch erc --severity-all --format json -o "$TMP/erc.json" m18-bruecke.kicad_sch >/dev/null 2>&1 || true
"$CLI" pcb drc --schematic-parity --severity-all --format json -o "$TMP/drc.json" m18-bruecke.kicad_pcb >/dev/null 2>&1 || true
python3 - "$TMP" <<'PY'
import json,sys,collections
d=json.load(open(sys.argv[1]+'/drc.json')); e=json.load(open(sys.argv[1]+'/erc.json'))
print('ERC:', sum(len(s['violations']) for s in e['sheets']))
print('DRC:', dict(collections.Counter(v['type'] for v in d['violations'])), 'offen:', len(d['unconnected_items']), 'Abgleich:', len(d.get('schematic_parity',[])))
for v in d['violations'][:10]: print('  ', v['type'], v['description'][:100], [i['description'][:50] for i in v['items']])
for u in d['unconnected_items'][:10]: print('   offen', [i['description'][:60] for i in u['items']])
PY
# Fertigungsdaten, Bilder, Bestellliste
rm -rf fertigung/gerber && mkdir -p fertigung/gerber bilder
"$CLI" pcb export gerbers -o fertigung/gerber/ -l F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts --subtract-soldermask m18-bruecke.kicad_pcb >/dev/null 2>&1
"$CLI" pcb export drill -o fertigung/gerber/ --format excellon --excellon-units mm --excellon-separate-th --generate-map --map-format pdf m18-bruecke.kicad_pcb >/dev/null 2>&1
rm -f fertigung/m18-bruecke-gerber.zip && (cd fertigung/gerber && zip -q ../m18-bruecke-gerber.zip *)
"$CLI" sch export pdf -o fertigung/m18-bruecke-schaltplan.pdf m18-bruecke.kicad_sch >/dev/null 2>&1
"$CLI" pcb export pdf -o fertigung/m18-bruecke-bestueckung-oben.pdf -l F.Fab,F.SilkS,Edge.Cuts --mode-single m18-bruecke.kicad_pcb >/dev/null 2>&1
"$CLI" pcb export pdf -o fertigung/m18-bruecke-bestueckung-unten.pdf -l B.Fab,B.SilkS,Edge.Cuts --mirror --mode-single m18-bruecke.kicad_pcb >/dev/null 2>&1
"$CLI" pcb render --side top --width 1600 --height 1100 --quality high -o bilder/platine-oben.png m18-bruecke.kicad_pcb >/dev/null 2>&1
"$CLI" pcb render --side bottom --width 1600 --height 1100 --quality high -o bilder/platine-unten.png m18-bruecke.kicad_pcb >/dev/null 2>&1
"$CLI" pcb render --side top --rotate "-35,0,25" --perspective --width 1600 --height 1100 --quality high -o bilder/platine-3d.png m18-bruecke.kicad_pcb >/dev/null 2>&1
python3 tools/bestellliste.py
ls fertigung
