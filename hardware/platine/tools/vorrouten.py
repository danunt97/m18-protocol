"""Legt die breiten Lade-/Akkuleitungen fest (gesperrt), bevor Freerouting den Rest routet."""
import pcbnew, os
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
b=pcbnew.LoadBoard(PCB)
F,B=pcbnew.F_Cu,pcbnew.B_Cu
xs=sorted(round(pcbnew.ToMM(p.GetPosition().x)-O[0],3) for p in b.FindFootprintByReference('X1').Pads())
xa,xb=xs[0],xs[-1]   # aeusserer und innerer Loetstift der Akkukontakte
yBP,yBM=b.FindFootprintByReference('X1').GetPosition().y, b.FindFootprintByReference('X4').GetPosition().y
yBP=pcbnew.ToMM(yBP)-O[1]; yBM=pcbnew.ToMM(yBM)-O[1]
tp1=b.FindFootprintByReference('TP1').GetPosition(); tp1=(pcbnew.ToMM(tp1.x)-O[0],pcbnew.ToMM(tp1.y)-O[1])
tp2=b.FindFootprintByReference('TP2').GetPosition(); tp2=(pcbnew.ToMM(tp2.x)-O[0],pcbnew.ToMM(tp2.y)-O[1])
def padpos(ref,num):
    p=[q for q in b.FindFootprintByReference(ref).Pads() if q.GetNumber()==num][0].GetPosition()
    return (round(pcbnew.ToMM(p.x)-O[0],3),round(pcbnew.ToMM(p.y)-O[1],3))
r5j=padpos('R5','2'); LK={n:padpos('LED1',n) for n in '134'}; LN={n:[q for q in b.FindFootprintByReference('LED1').Pads() if q.GetNumber()==n][0].GetNetname() for n in '134'}; RK2={r:padpos(r,'2') for r in ('R13','R15','R14')}
U10=padpos('U1','10'); J2S=padpos('J2','3'); J1S=padpos('J1','4'); U11=padpos('U1','11'); U12=padpos('U1','12'); tp3=padpos('TP3','1')
yM=pcbnew.ToMM(b.FindFootprintByReference('X5').GetPosition().y)-O[1]
yJ1=pcbnew.ToMM(b.FindFootprintByReference('X2').GetPosition().y)-O[1]
yJ2=pcbnew.ToMM(b.FindFootprintByReference('X3').GetPosition().y)-O[1]
# Siebdrucklinien oben (fuer die Wahl der festen Vias: Abstand wie die Sperrflaechen in sperrflaechen.py)
import math
SILK=[]
for f in b.GetFootprints():
    if f.IsFlipped(): continue
    for g in f.GraphicalItems():
        if g.GetLayer()!=pcbnew.F_SilkS or g.GetClass()!='PCB_SHAPE': continue
        a,e=g.GetStart(),g.GetEnd(); A=(pcbnew.ToMM(a.x)-O[0],pcbnew.ToMM(a.y)-O[1]); E=(pcbnew.ToMM(e.x)-O[0],pcbnew.ToMM(e.y)-O[1])
        if g.GetShape()==pcbnew.SHAPE_T_SEGMENT: SILK.append((A,E))
        elif g.GetShape()==pcbnew.SHAPE_T_RECT:
            P=[A,(E[0],A[1]),E,(A[0],E[1])]; SILK+=[(P[i],P[(i+1)%4]) for i in range(4)]
def dseg(p,a,c):
    vx,vy=c[0]-a[0],c[1]-a[1]; L=vx*vx+vy*vy
    t=0 if L==0 else max(0,min(1,((p[0]-a[0])*vx+(p[1]-a[1])*vy)/L))
    return math.hypot(p[0]-a[0]-t*vx,p[1]-a[1]-t*vy)
# Pads als Rechtecke (x0,y0,x1,y1) fuer den Abstandstest der festen Vias
PADS=[]
for f in b.GetFootprints():
    for p in f.Pads():
        q=p.GetPosition(); z=p.GetSize(); w,h=pcbnew.ToMM(z.x)/2,pcbnew.ToMM(z.y)/2
        if round(p.GetOrientationDegrees())%180==90: w,h=h,w
        x,y=pcbnew.ToMM(q.x)-O[0],pcbnew.ToMM(q.y)-O[1]; PADS.append((x-w,y-h,x+w,y+h))
YBL=[pcbnew.ToMM(b.FindFootprintByReference(r).GetPosition().y)-O[1] for r in ('X1','X2','X3','X4','X5')]
def drect(p,r): return math.hypot(max(r[0]-p[0],0,p[0]-r[2]),max(r[1]-p[1],0,p[1]-r[3]))
def frei(*kand,von=None,bw=0.5):
    """erster Kandidat, dessen Via (r 0,35) ausserhalb der Siebdruck-Sperren (0,6 um die Linie), weg von Pads (0,3)
    und ausserhalb der Sperrbaender unter den Messern liegt"""
    for k in kand:
        if any(dseg(k,a,c)<=0.5*1.415+0.4 for a,c in SILK if math.hypot(c[0]-a[0],c[1]-a[1])>=0.3): continue
        if any(drect(k,r)<0.35+0.3 for r in PADS): continue
        if k[0]<18.9+0.4 and any(abs(k[1]-y)<1.4+0.4 for y in YBL): continue
        if von:   # Zuleitung vom eigenen Pad darf keinem fremden Pad zu nahe kommen
            fremd=[r for r in PADS if drect(von,r)>0]
            pts=[(von[0]+(k[0]-von[0])*i/20,von[1]+(k[1]-von[1])*i/20) for i in range(21)]
            if any(drect(q,r)<bw/2+0.3 for q in pts for r in fremd): continue
        return k
    raise SystemExit('kein freier Via-Platz ab %s'%(kand[:1],))
def rund(c,rmax=2.5):
    """Kandidaten auf einem 0,1-mm-Raster um c, nach Abstand sortiert"""
    pts=[(round(c[0]+i*0.1,2),round(c[1]+j*0.1,2)) for i in range(-25,26) for j in range(-25,26)]
    return sorted((p for p in pts if math.hypot(p[0]-c[0],p[1]-c[1])<=rmax), key=lambda p:math.hypot(p[0]-c[0],p[1]-c[1]))
# erst jetzt aendern (KiCad-10-SWIG: nach Aenderungen liefern Abfragen untypisierte Zeiger)
ZONES=list(b.Zones())   # alle Flaechen entfernen; sperrflaechen.py und flaechen.py legen sie neu an
for t in list(b.GetTracks()): b.Remove(t)
def V(x,y): return pcbnew.VECTOR2I(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+y))
def track(net,pts,w,layer):
    n=b.FindNet(net)
    for a,c in zip(pts,pts[1:]):
        t=pcbnew.PCB_TRACK(b); t.SetStart(V(*a)); t.SetEnd(V(*c)); t.SetWidth(pcbnew.FromMM(w))
        t.SetLayer(layer); t.SetNet(n); t.SetLocked(True); b.Add(t)
# B+: Kontakt -> CHG+ (beide Lagen 2,5 mm), beide Kontaktstifte unten verbunden
# alles auf der Oberseite: unter den Messern (Unterseite) sind Leiterbahnen gesperrt. Die zwei Stifte eines Messers verbindet das Messer selbst.
bp=[(xb,yBP),(xb+3.7,tp1[1]),tp1]
track('/BAT+',bp,2.5,F)   # Weg zur Sicherung routet Freerouting
# B-: Kontakt -> CHG- oben 3 mm (dazu die GND-Flaeche)
track('GND',[(xb,yBM),(xb+3.7,tp2[1]),tp2],3.0,F)
def via(net,x,y):
    v=pcbnew.PCB_VIA(b); v.SetPosition(V(x,y)); v.SetWidth(pcbnew.FromMM(0.7)); v.SetDrill(pcbnew.FromMM(0.35))
    v.SetNet(b.FindNet(net)); v.SetLocked(True); b.Add(v)
# J1-Signal: oben vom inneren Stift schraeg nach unten, Via neben dem Sperrband, dann unten im freien Kanal
# zwischen den Messern J1 und M nach rechts (oben kreuzen dort die Versorgungsleitungen Sicherung -> Diode)
vj=frei((xb+1.6,yJ1+2.8),(xb+1.1,yJ1+3.2),(xb+0.9,yJ1+3.2))
track('/J1',[(xb,yJ1),vj],0.3,F); via('/J1',*vj)
track('/J1',[vj,(20.0,vj[1])],0.3,B)
# J2-Signal ebenso: Via oberhalb des Messers J2, unten im Kanal zwischen M und J2 nach rechts
vk=frei((xb+1.6,yJ2-2.8),(xb+1.1,yJ2-3.2),(xb+0.9,yJ2-3.2))
track('/J2',[(xb,yJ2),vk],0.3,F); via('/J2',*vk)
xv=frei(*[(x/10,vk[1]) for x in range(194,215)])[0]   # links neben dem Regler U2; oben weiter zu R5
track('/J2',[vk,(xv,vk[1])],0.3,B); via('/J2',xv,vk[1])
track('/J2',[(xv,vk[1]),(xv,r5j[1]+0.8),(r5j[0],r5j[1])],0.3,F)
# LED-Kathoden unten durch die Luecken der linken Stiftreihe des SuperMini zu R13/R15/R14 (Freerouting findet hier
# allein keinen Weg): rot zwischen Pin 11 und 12, blau zwischen Pin 10 und 11, gruen unterhalb von Pin 10
xp=U10[0]; yr=(U11[1]+U12[1])/2; yb=(U10[1]+U11[1])/2; yg=U10[1]+1.25
vR=frei(*[k for k in rund((RK2['R13'][0],RK2['R13'][1]-1.2),1.0) if k[1]<RK2['R13'][1]-0.9],von=RK2['R13'],bw=0.3)
vG=frei(*[k for k in rund((RK2['R15'][0],RK2['R15'][1]+1.4),1.0) if k[1]>RK2['R15'][1]+1.0],von=RK2['R15'],bw=0.3)
vB=frei(*[k for k in rund((RK2['R14'][0],RK2['R14'][1]+1.4),1.0) if k[1]>RK2['R14'][1]+1.0],von=RK2['R14'],bw=0.3)
r,g,bk=LK['1'],LK['3'],LK['4']
track(LN['1'],[r,(r[0]+0.8,r[1]-1.4),(xp-2.0,r[1]-1.4),(xp-1.2,yr),(xp+1.4,yr),vR],0.3,B); via(LN['1'],*vR)
track(LN['4'],[bk,(xp-1.6,yb),(xp+1.4,yb),(xp+2.3,yb+0.9),(vB[0]-1.1,yb+0.9),vB],0.3,B); via(LN['4'],*vB)
track(LN['3'],[g,(g[0],yg-0.6),(g[0]+0.6,yg),(vG[0]-0.6,yg),vG],0.3,B); via(LN['3'],*vG)
for n,v,ref in (('1',vR,'R13'),('3',vG,'R15'),('4',vB,'R14')): track(LN[n],[v,RK2[ref]],0.3,F)
# SDA zwischen den Steckerreihen J2 und J1 unten (dort kreuzen sich SDA/SCL und GND/3V3)
ym=(J2S[1]+J1S[1])/2
track('/SDA',[J2S,(J2S[0]+(J2S[1]-ym),ym),(J1S[0]-(ym-J1S[1])+0.8,ym),(J1S[0],J1S[1]+0.8),J1S],0.3,B)
# mittlerer Kontakt (X5) nur auf das Loetfeld TP3 zwischen seinen beiden Stiften
track('/AKKU_M',[(xa,yM),tp3,(xb,yM)],0.6,F)
for z in ZONES: b.Remove(z)
b.Save(PCB)
print('ok Vias', vj, vk, xv, vR, vG, vB, 'Kanaele', yr, yb, yg)
