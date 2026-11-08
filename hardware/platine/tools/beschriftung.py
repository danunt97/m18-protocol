"""Bestueckungsdruck aufraeumen: Werte auf F.Fab, Referenzen klein und kollisionsfrei,
Beschriftung der Akkukontakte. Rechtecke werden in Python gerechnet (KiCad-10-SWIG liefert
BOX2I aus Pads nicht zuverlaessig typisiert)."""
import pcbnew, os
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0); mm=pcbnew.FromMM; MM=pcbnew.ToMM
W,H=58.0,38.0
b=pcbnew.LoadBoard(PCB)
FP=list(b.GetFootprints())
# KiCad-10-SWIG: Objekte liefern nach Aenderungen am Board nur noch untypisierte Zeiger -> alles vorab holen
PADS={f.GetReference():list(f.Pads()) for f in FP}; GR={f.GetReference():list(f.GraphicalItems()) for f in FP}
REF={f.GetReference():f.Reference() for f in FP}; VAL={f.GetReference():f.Value() for f in FP}
FLIP={f.GetReference():f.IsFlipped() for f in FP}
CY={}
for f in FP:
    c=f.GetCourtyard(pcbnew.F_CrtYd).BBox(); CY[f.GetReference()]=(c.GetLeft(),c.GetTop(),c.GetRight(),c.GetBottom())
POS={f.GetReference():(f.GetPosition().x,f.GetPosition().y) for f in FP}
OLD=[d for d in b.GetDrawings() if d.GetClass()=='PCB_TEXT' and d.GetLayer() in (pcbnew.F_SilkS,pcbnew.B_SilkS)]
def R(x0,y0,x1,y1,m=0): return (min(x0,x1)-m,min(y0,y1)-m,max(x0,x1)+m,max(y0,y1)+m)
def hit(a,c): return not (a[2]<=c[0] or c[2]<=a[0] or a[3]<=c[1] or c[3]<=a[1])
pads=[]; silk=[]
for f in FP:
    for p in PADS[f.GetReference()]:
        x,y=MM(p.GetPosition().x)-O[0],MM(p.GetPosition().y)-O[1]
        s=p.GetSize(pcbnew.F_Cu); a=p.GetOrientationDegrees()%180
        w,h=(MM(s.x),MM(s.y)) if a<45 or a>135 else (MM(s.y),MM(s.x))
        pads.append((f.GetReference(),R(x-w/2,y-h/2,x+w/2,y+h/2,0.2)))
for f in FP:
    ref=f.GetReference()
    VAL[ref].SetLayer(pcbnew.B_Fab if FLIP[ref] else pcbnew.F_Fab)
    if ref.startswith(('H','X')): REF[ref].SetVisible(False)
    for g in GR[ref]:
        if g.GetLayer()!=pcbnew.F_SilkS: continue
        if g.GetClass()=='PCB_TEXT':
            continue
        st,en=g.GetStart(),g.GetEnd()
        r=R(MM(st.x)-O[0],MM(st.y)-O[1],MM(en.x)-O[0],MM(en.y)-O[1],0.1)
        if g.GetShapeStr() in ('Arc','Circle'):
            c=g.GetCenter(); rr=MM(g.GetRadius()); cx,cy=MM(c.x)-O[0],MM(c.y)-O[1]
            r=R(cx-rr,cy-rr,cx+rr,cy+rr,0.1)
        silk.append(r)
obst=[q for rf,q in pads]+silk
placed=[]
def free(r):
    if r[0]<0.6 or r[1]<0.6 or r[2]>W-0.6 or r[3]>H-0.6: return False
    return not any(hit(r,o) for o in obst+placed)
def textrect(cx,cy,n,size,ang):
    w=n*size*0.85+0.4; h=size+0.4
    if ang==90: w,h=h,w
    return R(cx-w/2,cy-h/2,cx+w/2,cy+h/2)
miss=[]
for f in sorted(FP,key=lambda f:f.GetReference()):
    t=REF[f.GetReference()]
    if f.GetReference().startswith(('H','X')) or FLIP[f.GetReference()]: continue
    size=0.8; t.SetTextSize(pcbnew.VECTOR2I(mm(size),mm(size))); t.SetTextThickness(mm(0.12)); t.SetLayer(pcbnew.F_SilkS)
    l,tp,rt,bt=CY[f.GetReference()]
    x0,y0,x1,y1=MM(l)-O[0],MM(tp)-O[1],MM(rt)-O[0],MM(bt)-O[1]
    cx,cyy=(x0+x1)/2,(y0+y1)/2; n=len(f.GetReference())
    cands=[]
    for ang in (0,90):
        tw=(n*size*0.85+0.4) if ang==0 else size+0.4
        th=size+0.4 if ang==0 else n*size*0.85+0.4
        for dx in (0,-1.5,1.5,-3,3):
            cands+= [(cx+dx,y0-th/2-0.05,ang),(cx+dx,y1+th/2+0.05,ang)]
        for dy in (0,-1.2,1.2,-2.4,2.4):
            cands+= [(x0-tw/2-0.05,cyy+dy,ang),(x1+tw/2+0.05,cyy+dy,ang)]
    for x,y,ang in cands:
        r=textrect(x,y,n,size,ang)
        if free(r):
            t.SetTextAngleDegrees(ang); t.SetPosition(pcbnew.VECTOR2I(mm(O[0]+x),mm(O[1]+y))); placed.append(r); break
    else:
        t.SetLayer(pcbnew.F_Fab); t.SetTextAngleDegrees(0); t.SetPosition(pcbnew.VECTOR2I(mm(O[0]+cx),mm(O[1]+cyy))); miss.append(f.GetReference())
def txt(s,x,y,size=1.0,layer=pcbnew.F_SilkS):
    t=pcbnew.PCB_TEXT(b); t.SetText(s); t.SetLayer(layer); t.SetTextSize(pcbnew.VECTOR2I(mm(size),mm(size))); t.SetTextThickness(mm(size*0.15))
    t.SetPosition(pcbnew.VECTOR2I(mm(O[0]+x),mm(O[1]+y)))
    if layer==pcbnew.B_SilkS: t.SetMirrored(True)
    b.Add(t)
for ref,name in (('X1','B+'),('X2','J1'),('X3','J2'),('X4','B-')):
    y=MM(POS[ref][1])-O[1]
    txt(name,7.0,y-2.2,1.2); txt(name,7.0,y-2.2,1.2,pcbnew.B_SilkS)
txt('M18-Bruecke',5.6,16.4,1.0)
txt('ESP32-S3 SuperMini',6.9,18.2,0.8)
txt('danunt97/m18-protocol',9.0,19.0,0.8,pcbnew.B_SilkS)
for d in OLD: b.Remove(d)
b.Save(PCB); print('Referenz nur auf F.Fab:',miss)
