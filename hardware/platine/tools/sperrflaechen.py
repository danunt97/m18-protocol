"""Sperrflaechen unter den Akkukontakten (Unterseite): keine Leiterbahnen, Vias und Kupferflaechen,
damit kein Messer ueber dem Loetstopplack auf einem fremden Netz aufliegt. Laeuft vor Freerouting."""
import pcbnew, os, math
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
b=pcbnew.LoadBoard(PCB)
# Siebdrucklinien der Bauteile oben (vorab holen, KiCad-10-SWIG): dort keine Vias, sonst druckt der Hersteller
# Bestueckungsdruck ueber ein Bohrloch (JLC-DFM 'Siebdruck zu Loch')
SEG=[]
for f in b.GetFootprints():
    if f.IsFlipped(): continue
    for g in f.GraphicalItems():
        if g.GetLayer()!=pcbnew.F_SilkS or g.GetClass()!='PCB_SHAPE': continue
        sh=g.GetShape()
        if sh==pcbnew.SHAPE_T_SEGMENT: SEG.append((g.GetStart(),g.GetEnd()))
        elif sh==pcbnew.SHAPE_T_RECT:
            c=g.GetCorners() if hasattr(g,'GetCorners') else None
            a,e=g.GetStart(),g.GetEnd()
            P=[(a.x,a.y),(e.x,a.y),(e.x,e.y),(a.x,e.y)]
            for i in range(4): SEG.append((pcbnew.VECTOR2I(*P[i]),pcbnew.VECTOR2I(*P[(i+1)%4])))
        elif sh in (pcbnew.SHAPE_T_ARC,pcbnew.SHAPE_T_CIRCLE):   # Boegen als Polygonzug
            c=g.GetCenter(); r=g.GetRadius()
            if sh==pcbnew.SHAPE_T_CIRCLE: a0,da=0.0,360.0
            else:
                a0=math.degrees(math.atan2(g.GetStart().y-c.y,g.GetStart().x-c.x)); da=g.GetArcAngle().AsDegrees()
            n=max(4,int(abs(da)/15))
            P=[(c.x+r*math.cos(math.radians(a0+da*i/n)),c.y+r*math.sin(math.radians(a0+da*i/n))) for i in range(n+1)]
            for i in range(n): SEG.append((pcbnew.VECTOR2I(int(P[i][0]),int(P[i][1])),pcbnew.VECTOR2I(int(P[i+1][0]),int(P[i+1][1]))))
SEG=[((s.x,s.y),(e.x,e.y)) for s,e in SEG]
YS=[pcbnew.ToMM(b.FindFootprintByReference(r).GetPosition().y)-O[1] for r in ('X1','X2','X3','X4','X5')]
for y in YS:
    k=pcbnew.ZONE(b); k.SetIsRuleArea(True); k.SetLayer(pcbnew.B_Cu)
    k.SetDoNotAllowZoneFills(True); k.SetDoNotAllowTracks(True); k.SetDoNotAllowVias(True)
    k.SetDoNotAllowPads(False); k.SetDoNotAllowFootprints(False)
    ol=k.Outline(); ol.NewOutline()
    for x,yy in ((0.6,y-1.4),(18.9,y-1.4),(18.9,y+1.4),(0.6,y+1.4)): ol.Append(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+yy))
    k.SetZoneName('Messer'); b.Add(k)
import math
H=pcbnew.FromMM(0.5)   # halbe Breite: Strich 0,12/2 + Via 0,35 + Abstand 0,1
for (x0,y0),(x1,y1) in SEG:
    L=math.hypot(x1-x0,y1-y0)
    if L<0.3: continue
    ux,uy=(x1-x0)/L,(y1-y0)/L; nx,ny=-uy*H,ux*H; ex,ey=ux*H,uy*H
    k=pcbnew.ZONE(b); k.SetIsRuleArea(True)
    ls=pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu); k.SetLayerSet(ls)
    k.SetDoNotAllowZoneFills(False); k.SetDoNotAllowTracks(False); k.SetDoNotAllowVias(True)
    k.SetDoNotAllowPads(False); k.SetDoNotAllowFootprints(False)
    ol=k.Outline(); ol.NewOutline()
    for px,py in ((x0-ex+nx,y0-ey+ny),(x1+ex+nx,y1+ey+ny),(x1+ex-nx,y1+ey-ny),(x0-ex-nx,y0-ey-ny)): ol.Append(int(px),int(py))
    k.SetZoneName('Siebdruck'); b.Add(k)
# Rand: 0,55 mm ohne Leiterbahnen und Vias (Freerouting kennt den Kantenabstand 0,4 mm sonst nicht)
W,H=float(os.environ.get('M18_W','58')),float(os.environ.get('M18_H','38')); R=0.55
for x0,y0,x1,y1 in ((0,0,W,R),(0,H-R,W,H),(0,0,R,H),(W-R,0,W,H)):
    k=pcbnew.ZONE(b); k.SetIsRuleArea(True)
    ls=pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu); k.SetLayerSet(ls)
    k.SetDoNotAllowZoneFills(False); k.SetDoNotAllowTracks(True); k.SetDoNotAllowVias(True)
    k.SetDoNotAllowPads(False); k.SetDoNotAllowFootprints(False)
    ol=k.Outline(); ol.NewOutline()
    for px,py in ((x0,y0),(x1,y0),(x1,y1),(x0,y1)): ol.Append(pcbnew.FromMM(O[0]+px),pcbnew.FromMM(O[1]+py))
    k.SetZoneName('Rand'); b.Add(k)
b.Save(PCB); print('Sperrflaechen', len(YS), 'Siebdruck-Via-Sperren', len(SEG))
