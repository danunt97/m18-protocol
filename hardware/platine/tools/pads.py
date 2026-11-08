"""Schreibt die Pad-Positionen (mm ab Platinenecke) nach tools/pads.json."""
import pcbnew, os, json
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
b=pcbnew.LoadBoard(PCB)
pads={}
for f in b.GetFootprints():
    for p in f.Pads():
        pads.setdefault(f.GetReference(),{}).setdefault(p.GetNumber(),[]).append((round(pcbnew.ToMM(p.GetPosition().x)-O[0],3), round(pcbnew.ToMM(p.GetPosition().y)-O[1],3), p.GetNetname()))
json.dump(pads,open(os.path.join(HERE,'pads.json'),'w'),indent=0)
