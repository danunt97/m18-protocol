import re, subprocess, collections, urllib.parse, os, tempfile
os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.comments import Comment
from openpyxl.utils import get_column_letter
CLI='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
net=os.path.join(tempfile.mkdtemp(),'n.net')
subprocess.run([CLI,'sch','export','netlist','-o',net,'m18-bruecke.kicad_sch'],check=True,capture_output=True)
n=open(net).read()
parts=collections.OrderedDict()
for m in re.finditer(r'\(comp\s*\(ref "([^"]+)"\)(.*?)\n\t\t\)', n, re.S):
    ref,blk=m.group(1),m.group(2)
    f=dict(re.findall(r'\(field\s*\(name "([^"]+)"\) "([^"]*)"\)',blk))
    val=re.search(r'\(value "([^"]*)"\)',blk).group(1)
    fp=re.search(r'\(footprint "([^"]*)"\)',blk).group(1)
    if 'MPN' not in f: continue
    key=f['MPN']
    p=parts.setdefault(key,dict(refs=[],val=val,fp=fp,**f)); p['refs'].append(ref)
def natkey(r): m=re.match(r'([A-Z]+)(\d+)',r); return (m.group(1),int(m.group(2)))
INFO={ # MPN: (Beschreibung, Bauform, Staffelmenge, Hinweis)
 'RC0805FR-072K2L':('Widerstand 2,2 kΩ 1 %','0805',100,''),
 'RC0805FR-074K7L':('Widerstand 4,7 kΩ 1 %','0805',100,''),
 'RC0805FR-0710KL':('Widerstand 10 kΩ 1 %','0805',100,''),
 'RC0805FR-071KL':('Widerstand 1 kΩ 1 %','0805',100,''),
 'RC0805FR-0715KL':('Widerstand 15 kΩ 1 %','0805',100,''),
 'RC0805FR-07100KL':('Widerstand 100 kΩ 1 %','0805',100,''),
 'RC0805FR-07270RL':('Widerstand 270 Ω 1 % (LED rot)','0805',100,''),
 'RC0805FR-07150RL':('Widerstand 150 Ω 1 % (LED grün/blau)','0805',100,''),
 'GRM31CR71H475KA12L':('Keramikkondensator 4,7 µF 50 V X7R','1206',0,''),
 'GRM31CR71E106KA12L':('Keramikkondensator 10 µF 25 V X7R','1206',0,''),
 'B1100-13-F':('Schottky-Diode 100 V 1 A (Verpolschutz)','SMA',0,''),
 'B140-13-F':('Schottky-Diode 40 V 1 A (5V-Entkopplung)','SMA',0,''),
 'SMBJ24A':('TVS-Diode 24 V unidirektional 600 W','SMB',0,''),
 '0452.500MRL':('SMD-Sicherung 500 mA träge, 125 V (NANO2)','2410 / NANO2',0,''),
 'MMBT3904LT1G':('NPN-Transistor 40 V 200 mA','SOT-23',100,''),
 'MMBT3906LT1G':('PNP-Transistor 40 V 200 mA','SOT-23',0,''),
 'R-78E5.0-0.5':('Schaltregler 5 V 0,5 A, Eingang 7–28 V','SIP3 THT',0,''),
 '928814-1':('FASTON-Flachstecker 6,35 × 0,81 mm, Leiterplatte (Akkukontakt)','THT',0,'4 Stück pro Platine'),
 'M20-7820942':('Buchsenleiste 1×9, 2,54 mm, 8,5 mm hoch (Sockel SuperMini)','THT',0,'2 Stück pro Platine'),
 'M20-7820442':('Buchsenleiste 1×4, 2,54 mm, 8,5 mm hoch (OLED)','THT',0,''),
 '521-8559F':('RGB-LED 5 mm, gemeinsame Anode','THT 5 mm',0,'Beinchen versetzt einlöten (Footprint mit versetzten Pads)'),
 '61300611121':('Stiftleiste 1×6, 2,54 mm (Lade-Steuerung)','THT',0,'Nicht bestücken (DNP), nur für spätere Ladeplatine'),
}
rows=[]
for mpn,p in parts.items():
    refs=sorted(p['refs'],key=natkey)
    per=len(refs)*(2 if mpn=='M20-7820942' else 1)
    d,bf,staffel,hint=INFO.get(mpn,(p['val'],'', 0,''))
    dnp = mpn=='61300611121'
    rows.append(dict(refs=', '.join(refs),desc=d,bf=bf,her=p.get('Hersteller','').split(' (')[0],mpn=mpn,mouser=p.get('Mouser',''),per=0 if dnp else per,staffel=staffel,hint=hint,dnp=dnp))
order={'X':0,'U':1,'F':2,'D':3,'C':4,'Q':5,'R':6,'L':7,'J':8}
rows.sort(key=lambda r:(order.get(r['refs'][0],9),natkey(r['refs'].split(',')[0])))
wb=Workbook(); ws=wb.active; ws.title='Bestellliste Mouser'
F=lambda **k: Font(name='Arial',**k)
thin=Side(style='thin',color='BBBBBB'); B=Border(left=thin,right=thin,top=thin,bottom=thin)
inp=Font(name='Arial',color='0000FF'); yellow=PatternFill('solid',fgColor='FFF2CC'); head=PatternFill('solid',fgColor='1F3864')
ws['A1']='M18-Brücke ESP32-S3 SuperMini – Bestellliste Mouser'; ws['A1'].font=F(bold=True,size=14)
ws['A3']='Anzahl Platinen'; ws['B3']=20; ws['B3'].font=inp; ws['B3'].fill=yellow
ws['A4']='Reserve'; ws['B4']=0.1; ws['B4'].number_format='0%'; ws['B4'].font=inp; ws['B4'].fill=yellow
ws['B4'].comment=Comment('Zuschlag für verlorene/defekte Teile beim Selbstlöten. Annahme, nach Bedarf ändern.','Claude')
ws['D3']='Blau/gelb = Eingabefelder. Preis und Lager bei Mouser prüfen und in die gelben Spalten eintragen; Summen rechnen sich selbst.'
ws['D4']='Link = Mouser-Suche nach der Herstellernummer (funktioniert auch, falls sich die Mouser-Nr. geändert hat).'
for c in ('A3','A4','D3','D4'): ws[c].font=F(size=9 if c[0]=='D' else 10, bold=c[0]=='A')
H=['Pos','Referenzen','Beschreibung','Bauform','Hersteller','Hersteller-Nr.','Mouser-Nr.','Stück / Platine','Bedarf','Staffel-/Mindestmenge','Bestellmenge','Stückpreis € (prüfen)','Summe €','Lager (prüfen)','Link','Hinweis']
hr=6
for i,h in enumerate(H,1):
    c=ws.cell(hr,i,h); c.font=F(bold=True,color='FFFFFF'); c.fill=head; c.alignment=Alignment(wrap_text=True,vertical='center'); c.border=B
ws.cell(hr,7).comment=Comment('Mouser-Bestellnummern nicht online geprüft. Der Link sucht nach der Herstellernummer; bei Abweichung die Nummer von der Mouser-Seite übernehmen.','Claude')
ws.cell(hr,10).comment=Comment('Annahme: bei 0805-Widerständen und MMBT3904 ist die 100er-Staffel kaum teurer als die Bedarfsmenge. Leer = Bedarf.','Claude')
r=hr+1
for k,row in enumerate(rows,1):
    url='https://www.mouser.de/c/?q='+urllib.parse.quote(row['mpn'])
    vals=[k,row['refs'],row['desc'],row['bf'],row['her'],row['mpn'],row['mouser'],row['per'],
          f'=ROUNDUP(H{r}*$B$3*(1+$B$4),0)', row['staffel'] or None, f'=MAX(I{r},J{r})', None, f'=K{r}*L{r}', None, 'Mouser öffnen', row['hint']]
    for i,v in enumerate(vals,1):
        c=ws.cell(r,i,v); c.font=F(); c.border=B; c.alignment=Alignment(vertical='top',wrap_text=i in (2,3,16))
    for col in (10,12,14): ws.cell(r,col).fill=yellow; ws.cell(r,col).font=inp
    ws.cell(r,8).font=inp
    l=ws.cell(r,15); l.hyperlink=url; l.font=Font(name='Arial',color='0563C1',underline='single')
    ws.cell(r,12).number_format='#,##0.000 €'; ws.cell(r,13).number_format='#,##0.00 €'
    if row['dnp']:
        for i in [x for x in range(1,17) if x!=15]: ws.cell(r,i).font=Font(name='Arial',color='808080',italic=True)
    r+=1
last=r-1
ws.cell(r,12,'Summe Mouser').font=F(bold=True)
ws.cell(r,13,f'=SUM(M{hr+1}:M{last})').font=F(bold=True); ws.cell(r,13).number_format='#,##0.00 €'
ws.cell(r+1,12,'pro Platine').font=F()
ws.cell(r+1,13,f'=IF($B$3>0,M{r}/$B$3,0)').number_format='#,##0.00 €'; ws.cell(r+1,13).font=F()
widths=[5,22,44,12,16,20,24,9,8,11,11,12,11,10,15,46]
for i,w in enumerate(widths,1): ws.column_dimensions[get_column_letter(i)].width=w
ws.row_dimensions[hr].height=32; ws.freeze_panes=ws.cell(hr+1,3)
ws.auto_filter.ref=f'A{hr}:P{last}'
# Blatt 2: nicht von Mouser
w2=wb.create_sheet('Sonstiges')
H2=['Teil','Stück / Platine','Bedarf','Bezugsquelle','Hinweis']
for i,h in enumerate(H2,1):
    c=w2.cell(1,i,h); c.font=F(bold=True,color='FFFFFF'); c.fill=head; c.border=B
other=[
 ('ESP32-S3 SuperMini (ESP32-S3FH4R2, 4 MB Flash, 2 MB PSRAM) mit Stiftleisten',1,'z. B. AliExpress/Amazon („ESP32-S3 SuperMini“)','Nicht bei Mouser. Stiftleisten liegen meist bei; Pinbelegung vor dem Bestellen prüfen (README).'),
 ('OLED-Display I²C 3,3 V, 4 Pins (GND, VCC, SCL, SDA)',1,'z. B. 0,96" SSD1306','Nicht bei Mouser. Pinreihenfolge muss zur Buchse passen (README).'),
 ('Leiterplatte 58 × 38 mm, 2 Lagen, 1,6 mm',1,'Leiterplattenhersteller (Gerber: fertigung/m18-bruecke-gerber.zip)','Mindestens 20 Stück bestellen.'),
 ('Schraube M3 (Befestigung)',4,'Baumarkt / Normteile','Länge je nach Gehäuse.'),
]
for k,(t,per,src,h) in enumerate(other,2):
    vals=[t,per,f"=ROUNDUP(B{k}*'Bestellliste Mouser'!$B$3*(1+'Bestellliste Mouser'!$B$4),0)",src,h]
    for i,v in enumerate(vals,1):
        c=w2.cell(k,i,v); c.font=F(); c.border=B; c.alignment=Alignment(wrap_text=True,vertical='top')
for i,w in enumerate([55,10,9,45,60],1): w2.column_dimensions[get_column_letter(i)].width=w
out='fertigung/m18-bruecke-bestellliste.xlsx'
wb.calculation.fullCalcOnLoad=True
wb.save(out); print(out, len(rows),'Positionen')
