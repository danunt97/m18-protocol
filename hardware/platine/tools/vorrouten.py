"""Legt die breiten Lade-/Akkuleitungen fest (gesperrt), bevor Freerouting den Rest routet."""
import pcbnew, os
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
b=pcbnew.LoadBoard(PCB)
F,B=pcbnew.F_Cu,pcbnew.B_Cu
xa,xb=12.176,17.256
yBP,yBM=b.FindFootprintByReference('X1').GetPosition().y, b.FindFootprintByReference('X4').GetPosition().y
yBP=pcbnew.ToMM(yBP)-O[1]; yBM=pcbnew.ToMM(yBM)-O[1]
tp1=b.FindFootprintByReference('TP1').GetPosition(); tp1=(pcbnew.ToMM(tp1.x)-O[0],pcbnew.ToMM(tp1.y)-O[1])
tp2=b.FindFootprintByReference('TP2').GetPosition(); tp2=(pcbnew.ToMM(tp2.x)-O[0],pcbnew.ToMM(tp2.y)-O[1])
f1=[p for p in b.FindFootprintByReference('F1').Pads() if p.GetNumber()=='1'][0].GetPosition(); f1=(pcbnew.ToMM(f1.x)-O[0],pcbnew.ToMM(f1.y)-O[1])
def padpos(ref,num):
    p=[q for q in b.FindFootprintByReference(ref).Pads() if q.GetNumber()==num][0].GetPosition()
    return (round(pcbnew.ToMM(p.x)-O[0],3),round(pcbnew.ToMM(p.y)-O[1],3))
r7b=padpos('R7','2'); q3b=padpos('Q3','1'); q5e=padpos('Q5','2')
# erst jetzt aendern (KiCad-10-SWIG: nach Aenderungen liefern Abfragen untypisierte Zeiger)
ZONES=list(b.Zones())   # Flaechen fuer Freerouting entfernen (flaechen.py legt sie danach neu an)
for t in list(b.GetTracks()): b.Remove(t)
def V(x,y): return pcbnew.VECTOR2I(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+y))
def track(net,pts,w,layer):
    n=b.FindNet(net)
    for a,c in zip(pts,pts[1:]):
        t=pcbnew.PCB_TRACK(b); t.SetStart(V(*a)); t.SetEnd(V(*c)); t.SetWidth(pcbnew.FromMM(w))
        t.SetLayer(layer); t.SetNet(n); t.SetLocked(True); b.Add(t)
# B+: Kontakt -> CHG+ (beide Lagen 2,5 mm), CHG+ -> Sicherung 1 mm, beide Kontaktstifte unten verbunden
track('/BAT+',[(xb,yBP),(tp1[0],tp1[1]-1.2),tp1],2.5,F)
track('/BAT+',[(xb,yBP),(tp1[0],tp1[1]-1.2),tp1],2.5,B)
track('/BAT+',[(xa,yBP),(xb,yBP)],1.5,B)
track('/BAT+',[tp1,(tp1[0]+2.0,f1[1]),f1],1.0,F)
# B-: Kontakt -> CHG- unten 3 mm (oben liegt R3 im Weg), beide Stifte unten verbunden
track('/GND',[(xb,yBM),(tp2[0],tp2[1]+1.2),tp2],3.0,B)
track('/GND',[(xa,yBM),(xb,yBM)],1.5,B)
def via(net,x,y):
    v=pcbnew.PCB_VIA(b); v.SetPosition(V(x,y)); v.SetWidth(pcbnew.FromMM(0.7)); v.SetDrill(pcbnew.FromMM(0.35))
    v.SetNet(b.FindNet(net)); v.SetLocked(True); b.Add(v)
# Q3B (R7 -> Basis Q3): Freerouting findet neben dem Emitter-Pad keinen Weg, deshalb kurz ueber die Unterseite
va=(r7b[0],r7b[1]+1.4); vb=(q3b[0]-1.7,q3b[1]-0.45)
track('/Q3B',[r7b,va],0.3,F); via('/Q3B',*va)
track('/Q3B',[va,(vb[0],va[1]-(va[0]-vb[0])),vb],0.3,B); via('/Q3B',*vb)
track('/Q3B',[vb,(q3b[0]-0.6,q3b[1]),q3b],0.3,F)
# Emitter Q5 an die GND-Flaeche unten (oben bleibt dort sonst eine Insel)
track('/GND',[q5e,(q5e[0]-1.15,q5e[1])],0.5,F); via('/GND',q5e[0]-1.15,q5e[1])
for z in ZONES: b.Remove(z)
b.Save(PCB)
print('ok', tp1, tp2, f1, yBP, yBM)
