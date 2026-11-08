"""Platziert alle Bauteile der M18-Bruecke (KiCad-Python, pcbnew).
Aufruf:  <KiCad>/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 tools/platzieren.py
Koordinaten in mm relativ zur linken oberen Platinenecke (O)."""
import pcbnew, os, sys
HERE=os.path.dirname(os.path.abspath(__file__)); PRJ=os.path.dirname(HERE)
PCB=os.path.join(PRJ,'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
W,H=float(os.environ.get('M18_W','58')),float(os.environ.get('M18_H','38'))
YC=H/2
# Akkukontakte: Abstaende aus dem alten Gehaeuse (Printables "Milwaukee M18 All Pins"), Mitte = YC
# Blick in Einsteckrichtung (Spitzen nach vorne, von oben): links B+, J1, J2, rechts B-  -> B+ unten
KONTAKT_Y={'X1':YC+10.55,'X2':YC+5.30,'X3':YC-5.20,'X4':YC-10.55}
KONTAKT_X=9.5   # Ursprung des TE-Footprints; Messer 0,66..16,66, Loetstifte bei 12,18 und 17,26
P={}  # ref -> (x,y,rot,side)
for r,y in KONTAKT_Y.items(): P[r]=(KONTAKT_X,y,180,'B')
exec(open(os.path.join(HERE,'lage.py')).read())   # fuellt P mit den uebrigen Bauteilen

def mm(x,y): return pcbnew.VECTOR2I(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+y))
b=pcbnew.LoadBoard(PCB)
FP=list(b.GetFootprints())
for f in FP:
    r=f.GetReference()
    if r not in P: print('NICHT PLATZIERT',r); continue
    x,y,rot,side=P[r]
    if side=='B' and not f.IsFlipped(): f.Flip(f.GetPosition(),pcbnew.FLIP_DIRECTION_LEFT_RIGHT if hasattr(pcbnew,'FLIP_DIRECTION_LEFT_RIGHT') else False)
    f.SetPosition(mm(x,y)); f.SetOrientationDegrees(rot)
# Umriss
for d in list(b.GetDrawings()):
    if d.GetLayer()==pcbnew.Edge_Cuts: b.Remove(d)
R=1.0
pts=[(0,0),(W,0),(W,H),(0,H)]
for i in range(4):
    s=pcbnew.PCB_SHAPE(b); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(pcbnew.FromMM(0.1))
    a,c=pts[i],pts[(i+1)%4]; s.SetStart(mm(*a)); s.SetEnd(mm(*c)); b.Add(s)
b.Save(PCB)
