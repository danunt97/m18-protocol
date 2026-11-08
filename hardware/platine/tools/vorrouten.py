"""Legt die breiten Lade-/Akkuleitungen fest (gesperrt), bevor Freerouting den Rest routet."""
import pcbnew, os
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
b=pcbnew.LoadBoard(PCB)
for t in list(b.GetTracks()): b.Remove(t)
def V(x,y): return pcbnew.VECTOR2I(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+y))
def track(net,pts,w,layer):
    n=b.FindNet(net)
    for a,c in zip(pts,pts[1:]):
        t=pcbnew.PCB_TRACK(b); t.SetStart(V(*a)); t.SetEnd(V(*c)); t.SetWidth(pcbnew.FromMM(w))
        t.SetLayer(layer); t.SetNet(n); t.SetLocked(True); b.Add(t)
F,B=pcbnew.F_Cu,pcbnew.B_Cu
xa,xb=12.176,17.256
yBP,yBM=b.FindFootprintByReference('X1').GetPosition().y, b.FindFootprintByReference('X4').GetPosition().y
yBP=pcbnew.ToMM(yBP)-O[1]; yBM=pcbnew.ToMM(yBM)-O[1]
tp1=b.FindFootprintByReference('TP1').GetPosition(); tp1=(pcbnew.ToMM(tp1.x)-O[0],pcbnew.ToMM(tp1.y)-O[1])
tp2=b.FindFootprintByReference('TP2').GetPosition(); tp2=(pcbnew.ToMM(tp2.x)-O[0],pcbnew.ToMM(tp2.y)-O[1])
f1=[p for p in b.FindFootprintByReference('F1').Pads() if p.GetNumber()=='1'][0].GetPosition(); f1=(pcbnew.ToMM(f1.x)-O[0],pcbnew.ToMM(f1.y)-O[1])
# B+: Kontakt -> CHG+ (beide Lagen 2,5 mm), CHG+ -> Sicherung 1 mm, beide Kontaktstifte unten verbunden
track('/BAT+',[(xb,yBP),(tp1[0],tp1[1]-1.2),tp1],2.5,F)
track('/BAT+',[(xb,yBP),(tp1[0],tp1[1]-1.2),tp1],2.5,B)
track('/BAT+',[(xa,yBP),(xb,yBP)],1.5,B)
track('/BAT+',[tp1,(tp1[0]+2.0,f1[1]),f1],1.0,F)
# B-: Kontakt -> CHG- unten 3 mm (oben liegt R3 im Weg), beide Stifte unten verbunden
track('/GND',[(xb,yBM),(tp2[0],tp2[1]+1.2),tp2],3.0,B)
track('/GND',[(xa,yBM),(xb,yBM)],1.5,B)
b.Save(PCB)
print('ok', tp1, tp2, f1, yBP, yBM)
