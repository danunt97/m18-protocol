"""Zeichnet den Schaltplan der M18-Bruecke lesbar neu: Funktionsbloecke mit Rahmen und Ueberschrift, Bauteile im Block
direkt verdrahtet, GND/+5V/+3V3 als Versorgungssymbole, Verbindungen zwischen den Bloecken ueber Netznamen.
Bauteile, Felder (Hersteller, MPN, Mouser ...) und UUIDs bleiben erhalten; nur Lage, Leitungen und Texte werden neu gesetzt.
Danach mit tools/netze_umbenennen.py die Netznamen der Platine nachziehen.
Aufruf aus hardware/platine:  python3 tools/schaltplan.py"""
import os, sys, math, uuid, re
HERE=os.path.dirname(os.path.abspath(__file__)); PRJ=os.path.dirname(HERE)
SCH=os.path.join(PRJ,'m18-bruecke.kicad_sch')
POWERLIB='/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/power.kicad_sym'

# ---------------------------------------------------------------- S-Ausdruecke
TOK=re.compile(r'\s*(\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+)')
class Sym(str): pass
def parse(s):
    st=[[]]
    for m in TOK.finditer(s):
        t=m.group(1)
        if t=='(': st.append([])
        elif t==')': x=st.pop(); st[-1].append(x)
        elif t[0]=='"': st[-1].append(re.sub(r'\\(.)',lambda m:{'n':'\n','\\':'\\','"':'"'}.get(m.group(1),m.group(1)),t[1:-1]))
        else: st[-1].append(Sym(t))
    return st[0][0]
def qs(s): return '"'+s.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n')+'"'
def num(v): s=('%.4f'%v).rstrip('0').rstrip('.'); return '0' if s in ('-0','') else s
def dump(x,ind=0):
    if isinstance(x,list):
        if all(not isinstance(e,list) for e in x): return '('+' '.join(dump(e) for e in x)+')'
        out='('+' '.join(dump(e) for e in x if not isinstance(e,list))
        for e in x:
            if isinstance(e,list): out+='\n'+'\t'*(ind+1)+dump(e,ind+1)
        return out+'\n'+'\t'*ind+')'
    if isinstance(x,Sym): return str(x)
    if isinstance(x,(int,float)): return num(x)
    return qs(x)
S=Sym
def find(x,k): return [e for e in x if isinstance(e,list) and e and e[0]==k]
def get(x,k):
    r=find(x,k); return r[0] if r else None
def U(): return [S('uuid'),str(uuid.uuid4())]

t=parse(open(SCH).read())
ROOT=get(t,'uuid')[1]
lib=get(t,'lib_symbols')

# Versorgungssymbole aus der KiCad-Bibliothek uebernehmen
pl=parse(open(POWERLIB).read())
have={s[1] for s in find(lib,'symbol')}
for n in ('GND','+5V','+3V3'):
    if 'power:'+n in have: continue
    s=[e for e in find(pl,'symbol') if e[1]==n][0]; s=list(s); s[1]='power:'+n; lib.append(s)

for s_ in find(lib,'symbol'):
    if s_[1]=='m18:LED_RGB_521-8559F':
        def ren(x):
            for e in x:
                if isinstance(e,list):
                    if e and e[0]=='name' and e[1] in ('RED_K','GREEN_K','BLUE_K','COMMON_A'):
                        e[1]={'RED_K':'K_R','GREEN_K':'K_G','BLUE_K':'K_B','COMMON_A':'A'}[e[1]]
                    else: ren(e)
        ren(s_)
# Pin-Lage aus der Bibliothek (y nach oben) in den Schaltplan (y nach unten)
PINS={}
for s in find(lib,'symbol'):
    pins={}
    def walk(x):
        for e in x:
            if isinstance(e,list):
                if e and e[0]=='pin':
                    at=get(e,'at'); pins[get(e,'number')[1]]=(float(at[1]),float(at[2]))
                else: walk(e)
    walk(s); PINS[s[1]]=pins
def xf(px,py,X,Y,rot,mir=None):
    x,y=px,-py
    if mir=='x': y=-y
    if mir=='y': x=-x
    a=math.radians(rot); xr=x*math.cos(a)+y*math.sin(a); yr=-x*math.sin(a)+y*math.cos(a)
    return (round(X+xr,3),round(Y+yr,3))

INST={}
for s in find(t,'symbol'):
    ref=[p[2] for p in find(s,'property') if p[1]=='Reference'][0]; INST[ref]=s
PLACED={}
def place(ref,x,y,rot=0,mir=None,fields=None):
    """setzt Bauteil ref an (x,y); fields: {'Reference':(dx,dy),'Value':(dx,dy)} relativ, Text waagerecht und mittig"""
    s=INST[ref]; at=get(s,'at'); at[1],at[2],at[3]=x,y,rot
    for m in find(s,'mirror'): s.remove(m)
    if mir: s.insert(s.index(at)+1,[S('mirror'),S(mir)])
    PLACED[ref]=(x,y,rot,mir)
    for p in find(s,'property'):
        pa=get(p,'at')
        hidden=any(isinstance(e,list) and e[0]=='hide' and e[1]=='yes' for e in p) or \
               any(isinstance(e,list) and e[0]=='hide' for ef in find(p,'effects') for e in ef)
        f=(fields or {}).get(p[1])
        if f is None and not hidden:
            p.insert(p.index(pa)+1,[S('hide'),S('yes')]); hidden=True
        if f is None or hidden: pa[1],pa[2],pa[3]=x,y,0; continue
        pa[1],pa[2],pa[3]=round(x+f[0],3),round(y+f[1],3),(90 if rot in (90,270) else 0)
        for ef in find(p,'effects'):
            for j in find(ef,'justify'): ef.remove(j)
            if len(f)>2: ef.append([S('justify')]+[S(w) for w in f[2].split()])
def pin(ref,n):
    x,y,rot,mir=PLACED[ref]; lid=get(INST[ref],'lib_id')[1]; px,py=PINS[lid][n]
    return xf(px,py,x,y,rot,mir)

# ---------------------------------------------------------------- Zeichenelemente
NEW=[]
def wire(*pts):
    for a,b in zip(pts,pts[1:]):
        if a==b: continue
        NEW.append([S('wire'),[S('pts'),[S('xy'),a[0],a[1]],[S('xy'),b[0],b[1]]],
                    [S('stroke'),[S('width'),0],[S('type'),S('default')]],U()])
def junction(p):
    NEW.append([S('junction'),[S('at'),p[0],p[1]],[S('diameter'),0],[S('color'),0,0,0,0],U()])
def label(name,p,ang=0):
    just={0:'left bottom',180:'right bottom',90:'left bottom',270:'right bottom'}[ang]
    NEW.append([S('label'),name,[S('at'),p[0],p[1],ang],
                [S('effects'),[S('font'),[S('size'),1.27,1.27]],[S('justify')]+[S(w) for w in just.split()]],U()])
def nc(p): NEW.append([S('no_connect'),[S('at'),p[0],p[1]],U()])
def text(s,x,y,size=1.27,bold=False,ang=0):
    font=[S('font'),[S('size'),size,size]]+([[S('bold'),S('yes')]] if bold else [])
    NEW.append([S('text'),s,[S('exclude_from_sim'),S('no')],[S('at'),x,y,ang],
                [S('effects'),font,[S('justify'),S('left'),S('bottom')]],U()])
def box(x0,y0,x1,y1,title):
    NEW.append([S('rectangle'),[S('start'),x0,y0],[S('end'),x1,y1],
                [S('stroke'),[S('width'),0.15],[S('type'),S('dash')],[S('color'),72,72,72,1]],[S('fill'),[S('type'),S('none')]],U()])
    text(title,x0+1.27,y0+3.81,1.8,True)
NPWR=[0]
TEMPLATE=INST['#FLG01'] if '#FLG01' in INST else None
def power(kind,p,rot=0):
    """Versorgungssymbol (GND, +5V, +3V3) mit dem Anschluss bei p. rot: 0 = Pfeil/Balken nach oben (GND nach unten)"""
    NPWR[0]+=1; ref='#PWR%02d'%NPWR[0]
    vy={'GND':-3.81,'+5V':3.556,'+3V3':3.556}[kind]
    vx_,vy_=xf(0,vy,p[0],p[1],rot); va=0
    if rot in (90,270):   # Pfeil/Balken zeigt nach rechts: Text waagerecht dahinter
        d={'GND':4.83,'+5V':5.33,'+3V3':5.84}[kind]; vx_,vy_=p[0]+d,p[1]; va=rot
    NEW.append([S('symbol'),[S('lib_id'),'power:'+kind],[S('at'),p[0],p[1],rot],[S('unit'),1],[S('body_style'),1],
        [S('exclude_from_sim'),S('no')],[S('in_bom'),S('yes')],[S('on_board'),S('yes')],[S('in_pos_files'),S('yes')],[S('dnp'),S('no')],U(),
        [S('property'),'Reference',ref,[S('at'),p[0],p[1],0],[S('hide'),S('yes')],[S('show_name'),S('no')],[S('do_not_autoplace'),S('no')],
            [S('effects'),[S('font'),[S('size'),1.27,1.27]]]],
        [S('property'),'Value',kind,[S('at'),vx_,vy_,va],[S('show_name'),S('no')],[S('do_not_autoplace'),S('no')],
            [S('effects'),[S('font'),[S('size'),1.27,1.27]]]],
        [S('property'),'Footprint','',[S('at'),p[0],p[1],0],[S('hide'),S('yes')],[S('show_name'),S('no')],[S('do_not_autoplace'),S('no')],
            [S('effects'),[S('font'),[S('size'),1.27,1.27]]]],
        [S('property'),'Datasheet','',[S('at'),p[0],p[1],0],[S('hide'),S('yes')],[S('show_name'),S('no')],[S('do_not_autoplace'),S('no')],
            [S('effects'),[S('font'),[S('size'),1.27,1.27]]]],
        [S('property'),'Description','Versorgungssymbol',[S('at'),p[0],p[1],0],[S('hide'),S('yes')],[S('show_name'),S('no')],[S('do_not_autoplace'),S('no')],
            [S('effects'),[S('font'),[S('size'),1.27,1.27]]]],
        [S('pin'),'1',U()],
        [S('instances'),[S('project'),'m18-bruecke',[S('path'),'/'+ROOT,[S('reference'),ref],[S('unit'),1]]]]])
def gnd(p): power('GND',p)
def flag(ref,p,rot=0): place(ref,p[0],p[1],rot,fields={'Value':(0,-3.81) if rot==0 else (0,3.81)})

g=2.54
R_V={'Reference':(2.54,-1.27,'left'),'Value':(2.54,1.27,'left')}      # Widerstand/Kondensator senkrecht
R_H={'Reference':(0,-2.54),'Value':(0,2.54)}                           # waagerecht
Q_F={'Reference':(4.45,-4.83,'left'),'Value':(4.45,-2.54,'left')}        # Transistor, Text rechts
Q_FM={'Reference':(5.08,-1.27,'left'),'Value':(5.08,1.27,'left')}
D_H={'Reference':(0,-3.175),'Value':(0,3.175)}

# ================================================================ 1) Akkuanschluss
box(12.7,12.7,76.2,107.95,'Akkuanschluss (Unterseite, TE 928814-1)')
K_X=25.4; KF={'Reference':(-5.08,-1.27,'right'),'Value':(-5.08,1.27,'right')}
place('X1',K_X,30.48,fields=KF); place('TP1',K_X,38.1,fields=KF)
wire(pin('X1','1'),(48.26,30.48)); label('BAT+',(48.26,30.48))
wire(pin('TP1','1'),(40.64,38.1),(40.64,30.48)); junction((40.64,30.48))
place('X2',K_X,50.8,fields=KF); wire(pin('X2','1'),(48.26,50.8)); label('J1',(48.26,50.8))
place('X5',K_X,60.96,fields=KF); place('TP3',45.72,60.96,180,fields={'Reference':(5.08,-1.27,'left'),'Value':(5.08,1.27,'left')})
wire(pin('X5','1'),pin('TP3','1')); label('AKKU_M',(31.75,60.96))
place('X3',K_X,71.12,fields=KF); wire(pin('X3','1'),(48.26,71.12)); label('J2',(48.26,71.12))
place('X4',K_X,81.28,fields=KF); place('TP2',K_X,88.9,fields=KF)
wire(pin('X4','1'),(40.64,81.28),(40.64,91.44)); wire(pin('TP2','1'),(40.64,88.9)); junction((40.64,88.9)); gnd((40.64,91.44))
flag('#FLG01',(45.72,81.28)); wire((40.64,81.28),(45.72,81.28)); junction((40.64,81.28))
text('Reihenfolge von oben in Einsteckrichtung:',15.24,100.33,1.27)
text('links B+, J1, M, J2, rechts B-  (Raster 5,625 mm)',15.24,102.87,1.27)
text('M: Funktion unbekannt, nur auf Loetfeld TP3',15.24,105.41,1.27)
text('TP1/TP2: Lade-Anschluss, ~5 A, nicht abgesichert',15.24,22.86,1.27)

# ================================================================ 2) Versorgung
box(80.01,12.7,205.74,58.42,'Versorgung: Akku 15-21 V -> 5 V')
YV=30.48
label('BAT+',(86.36,YV),180); place('F1',95.25,YV,90,fields=R_H); wire((86.36,YV),pin('F1','1'))
place('D1',110.49,YV,180,fields={'Reference':(0,-3.175),'Value':(0,3.175)})
wire(pin('F1','2'),pin('D1','2')); label('VBAT_F',(100.33,YV))
place('D2',124.46,38.1,270,fields={'Reference':(2.54,-1.27,'left'),'Value':(2.54,1.27,'left')})
place('C1',137.16,38.1,0,fields=R_V)
place('U2',153.67,YV,0,fields={'Reference':(0,-8.89),'Value':(0,-6.35)})
wire(pin('D1','1'),pin('U2','1'))
for r in ('D2','C1'):
    top=pin(r,'1'); wire((top[0],YV),top); junction((top[0],YV)); gnd(pin(r,'2'))
label('VBAT',(115.57,YV)); flag('#FLG02',(142.24,YV)); junction((142.24,YV))
gnd(pin('U2','2'))
place('C2',168.91,38.1,0,fields=R_V); place('D3',181.61,YV,180,fields={'Reference':(0,-3.175),'Value':(0,3.175)})
wire(pin('U2','3'),pin('D3','2')); top=pin('C2','1'); wire((top[0],YV),top); junction((top[0],YV)); gnd(pin('C2','2'))
label('+5V_REG',(161.29,YV))
k=pin('D3','1'); wire(k,(199.39,YV)); power('+5V',(199.39,YV)); flag('#FLG03',(189.23,YV)); junction((189.23,YV))
text('F1 T500mA, D1 Verpolschutz, D2 TVS 24 V, U2 RECOM R-78E5.0-0.5',82.55,50.8,1.27)
text('D3 trennt USB-5V vom Regler (am 5V-Pin ~4,6 V aus dem Akku)',82.55,53.34,1.27)

# ================================================================ 3) ESP32-S3 SuperMini
box(210.82,12.7,287.02,73.66,'ESP32-S3 SuperMini (direkt eingeloetet)')
UX,UY=248.92,43.18
place('U1',UX,UY,0,fields={'Reference':(0,-13.97),'Value':(0,13.97)})
L=7.62
left={'6':'GPIO4_RES','7':'CHG_EN','8':'RX','9':'TX'}
for n in [str(i) for i in range(1,10)]:
    p=pin('U1',n)
    if n in left: wire(p,(p[0]-L,p[1])); label(left[n],(p[0]-L,p[1]),180)
    else: nc(p)
right={'13':'LED_R','14':'LED_B','15':'LED_G','17':'SCL','18':'SDA'}
for n in [str(i) for i in range(10,19)]:
    p=pin('U1',n); e=(p[0]+L,p[1])
    if n in right: wire(p,e); label(right[n],e)
    elif n=='10': wire(p,e); power('+5V',e,270)
    elif n=='11': wire(p,e); power('GND',e,90)
    elif n=='12': wire(p,e); power('+3V3',e,270)
    else: nc(p)
text('TX = GPIO7, RX = GPIO6, LED R/B/G = GPIO13/12/11',213.36,63.5,1.27)
text('CHG_EN = GPIO5, Reserve = GPIO4, I2C SDA/SCL = GPIO8/9',213.36,66.04,1.27)
text('USB-C zeigt zur Platinenkante (Seite B-)',213.36,68.58,1.27)

# ================================================================ 4) RGB-LED
box(80.01,63.5,207.01,109.22,'RGB-LED (gemeinsame Anode, Lumetheus 5 mm)')
LX,LY=194.31,78.74
place('LED1',LX,LY,0,fields={'Reference':(0,-6.35)})
a=pin('LED1','2'); wire(a,(a[0]+2.54,a[1]),(a[0]+2.54,a[1]-5.08)); power('+5V',(a[0]+2.54,a[1]-5.08))
QY=95.25
for i,(gp,rb,q,rs,kp) in enumerate((('LED_R','R16','Q5','R13','1'),('LED_B','R17','Q6','R14','4'),('LED_G','R18','Q7','R15','3'))):
    qx=110.49+i*33.02
    place(q,qx,QY,0,fields={'Reference':Q_F['Reference']}); place(rb,qx-11.43,QY,90,fields=R_H)
    wire(pin(rb,'2'),pin(q,'1')); label(gp,pin(rb,'1'),180)
    gnd(pin(q,'2'))
    c=pin(q,'3'); place(rs,c[0],c[1]-6.35,0,mir='x',fields=R_V); wire(pin(rs,'1'),c)   # Pad 1 zum Kollektor wie auf der Platine
    top=pin(rs,'2'); kk=pin('LED1',kp)
    wire(top,(top[0],kk[1]),kk)
text('Q5-Q7 MMBT3904, Basis 2k2, Vorwiderstand rot 270 R, gruen/blau 150 R: ca. 7-9 mA',82.55,107.95,1.27)

# ================================================================ 5) J2 senden
box(12.7,111.76,104.14,167.64,'J2 senden (nicht invertierend, High = VBAT)')
YB=138.43
label('TX',(20.32,YB),180); place('R1',30.48,YB,90,fields=R_H); wire((20.32,YB),pin('R1','1'))
place('Q1',45.72,YB,0,fields=Q_F); wire(pin('R1','2'),pin('Q1','1'))
place('R2',36.83,144.78,0,fields=R_V); b2=pin('R2','1'); wire((b2[0],YB),b2); junction((b2[0],YB)); gnd(pin('R2','2'))
gnd(pin('Q1','2'))
YQ=128.27
place('Q2',73.66,YQ,0,mir='x',fields=Q_FM)
place('R4',58.42,YQ,90,fields=R_H)
c1=pin('Q1','3'); wire(c1,(c1[0],YQ),pin('R4','1')); wire(pin('R4','2'),pin('Q2','1'))
place('R3',64.77,124.46,0,fields=R_V); r3b=pin('R3','2'); wire(r3b,(r3b[0],YQ)); junction((r3b[0],YQ))
YR=119.38
e2=pin('Q2','2'); r3t=pin('R3','1')
wire(r3t,(r3t[0],YR)); wire(e2,(e2[0],YR)); wire((r3t[0],YR),(e2[0],YR),(86.36,YR)); junction((e2[0],YR)); label('VBAT',(86.36,YR))
c2=pin('Q2','3'); place('R5',83.82,YB,90,fields=R_H); wire(c2,(c2[0],YB),pin('R5','1'))
place('R6',91.44,144.78,0,fields=R_V); r6=pin('R6','1'); wire(pin('R5','2'),(97.79,YB)); wire((r6[0],YB),r6); junction((r6[0],YB))
gnd(pin('R6','2')); label('J2',(97.79,YB))
text('Ruhe: Q1/Q2 aus, R6 zieht J2 auf B-',15.24,162.56,1.27)
text('TX high: Q1 an -> Q2 an -> J2 = VBAT ueber R5 1k',15.24,165.1,1.27)

# ================================================================ 6) J1 empfangen
box(107.95,111.76,205.74,167.64,'J1 empfangen (Spud-Isolator, nicht invertierend)')
YJ=146.05
label('J1',(115.57,YJ),180); place('R7',125.73,YJ,90,fields=R_H); wire((115.57,YJ),pin('R7','1'))
place('Q3',140.97,YJ,0,fields=Q_F); wire(pin('R7','2'),pin('Q3','1'))
place('R8',132.08,152.4,0,fields=R_V); b=pin('R8','1'); wire((b[0],YJ),b); junction((b[0],YJ)); gnd(pin('R8','2')); gnd(pin('Q3','2'))
YK=135.89
place('Q4',166.37,YK,0,fields=Q_F); c3=pin('Q3','3'); wire(c3,(c3[0],YK),pin('Q4','1'))
place('R9',152.4,128.27,0,fields=R_V); r9=pin('R9','2'); wire(r9,(r9[0],YK)); junction((r9[0],YK)); power('+3V3',pin('R9','1'))
gnd(pin('Q4','2'))
YX=125.73
c4=pin('Q4','3'); wire(c4,(c4[0],YX),(198.12,YX)); label('RX',(198.12,YX))
place('R10',189.23,120.65,0,fields=R_V); r10=pin('R10','2'); wire(r10,(r10[0],YX)); junction((r10[0],YX)); power('+3V3',pin('R10','1'))
text('J1 high: Q3 an -> Q4 aus -> RX high (R10)',110.49,162.56,1.27)
text('R8 haelt Q3 bei offenem Kontakt aus',110.49,165.1,1.27)

# ================================================================ 7) OLED und Lade-Steuerung
box(210.82,78.74,287.02,160.02,'OLED (I2C) und Lade-Steuerung')
def conn(ref,x,y,names,ttl):
    place(ref,x,y,0,fields={'Reference':(0,-8.89 if ref=='J2' else -6.35),'Value':(0,13.97 if ref=='J2' else 11.43)})
    for n,nm in names.items():
        p=pin(ref,n); e=(p[0]+5.08,p[1])
        if nm=='GND': wire(p,e); power('GND',e,90)
        elif nm=='+3V3': wire(p,e); power('+3V3',e,270)
        else: wire(p,e); label(nm,e)
conn('J1',220.98,93.98,{'1':'GND','2':'+3V3','3':'SCL','4':'SDA'},'OLED')
text('J1: Buchse Harwin M20-7820442, OLED 0,96" (GND VCC SCL SDA)',213.36,113.03,1.27)
for i,(r,nm) in enumerate((('R12','SCL'),('R11','SDA'))):
    x=257.81+i*12.7
    place(r,x,93.98,0,fields=R_V); power('+3V3',pin(r,'1'))
    bt=pin(r,'2'); wire(bt,(bt[0],bt[1]+5.08)); label(nm,(bt[0],bt[1]+5.08),270)
text('R11/R12: I2C-Pull-ups 4k7',213.36,115.57,1.27)
conn('J2',220.98,128.27,{'1':'+3V3','2':'GND','3':'SDA','4':'SCL','5':'CHG_EN','6':'GPIO4_RES'},'Lade')
text('J2: Steuerstecker fuer eine spaetere Ladeplatine (nicht bestueckt)',213.36,152.4,1.27)
text('CHG_EN: GPIO5, low = Laden aus',213.36,154.94,1.27)

# ================================================================ 8) Befestigung
box(212.09,163.83,220.98,163.83+0.01,'')  # Platzhalter (wird entfernt)
NEW.pop(); NEW.pop()
for i,h in enumerate(('H1','H2','H3','H4')):
    place(h,15.24+i*10.16,180.34,0,fields={'Reference':(0,-3.81),'Value':(0,3.81)})
text('Befestigung 4 x M3, Lochabstand 50 x 30 mm',12.7,172.72,1.27,True)

# ---------------------------------------------------------------- alte Leitungen, Netznamen und Texte ersetzen
drop=('wire','label','junction','no_connect','text','rectangle','global_label','bus','polyline')
t[:]=[e for e in t if not (isinstance(e,list) and e and e[0] in drop)]
# alte Versorgungssymbole (#PWR) entfernen
t[:]=[e for e in t if not (isinstance(e,list) and e and e[0]=='symbol' and get(e,'lib_id') and get(e,'lib_id')[1].startswith('power:') and get(e,'lib_id')[1]!='power:PWR_FLAG')]
# nicht benoetigte PWR_FLAG entfernen
for r in [r for r in INST if r.startswith('#FLG') and r not in PLACED]: t.remove(INST[r])
nicht=[r for r in INST if r not in PLACED and not r.startswith('#PWR') and not r.startswith('#FLG')]
if nicht: sys.exit('nicht platziert: %s'%nicht)
i=t.index(get(t,'sheet_instances'))
t[i:i]=NEW
tb=get(t,'title_block')
neu_tb=[S('title_block'),[S('title'),'M18-Bruecke mit ESP32-S3 SuperMini'],[S('date'),'2026-11-08'],[S('rev'),'1.1'],
        [S('company'),'danunt97/m18-protocol'],[S('comment'),1,'Schaltplan neu gezeichnet: Bloecke, Versorgungssymbole'],
        [S('comment'),2,'Platine: hardware/platine/m18-bruecke.kicad_pcb']]
if tb: t[t.index(tb)]=neu_tb
else: t.insert(t.index(get(t,'paper'))+1,neu_tb)
open(SCH,'w').write(dump(t)+'\n')
print('ok', len(PLACED),'Bauteile,', sum(1 for e in NEW if e[0]=='wire'),'Leitungen,', NPWR[0],'Versorgungssymbole')
