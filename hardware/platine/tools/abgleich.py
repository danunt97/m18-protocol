"""Gleicht die Platine an den Schaltplan an (wie 'Platine aus Schaltplan aktualisieren'):
Netznamen mit '/'-Praefix, unbeschaltete Pads, Bestellfelder, DNP/BOM-Attribute.
Nutzt die von kicad-cli exportierte Netzliste. Reines Python, kein pcbnew."""
import re, os, subprocess, uuid, json, tempfile
HERE=os.path.dirname(os.path.abspath(__file__)); PRJ=os.path.dirname(HERE)
PCB=os.path.join(PRJ,'m18-bruecke.kicad_pcb'); SCH=os.path.join(PRJ,'m18-bruecke.kicad_sch'); PRO=os.path.join(PRJ,'m18-bruecke.kicad_pro')
CLI='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
net=os.path.join(tempfile.mkdtemp(),'n.net')
subprocess.run([CLI,'sch','export','netlist','-o',net,SCH],check=True,capture_output=True)
n=open(net).read()
# Bauteile und Felder
comps={}
for m in re.finditer(r'\(comp\s*\(ref "([^"]+)"\)(.*?)\n\t\t\)', n, re.S):
    ref,blk=m.group(1),m.group(2)
    f={k:v for k,v in re.findall(r'\(field\s*\(name "([^"]+)"\) "([^"]*)"\)',blk)}
    ds=re.search(r'\(datasheet "([^"]*)"\)',blk)
    if ds and ds.group(1) not in ('','~'): f['Datasheet']=ds.group(1)
    d=re.search(r'\(description "([^"]*)"\)',blk)
    v=re.search(r'\(value "([^"]*)"\)',blk); f['_value']=v.group(1) if v else None
    f['Description']=d.group(1) if d else ''
    comps[ref]=f
# Netze
nets={}
for m in re.finditer(r'\(net\s*\(code "\d+"\)\s*\(name "([^"]+)"\)(.*?)\n\t\t\)', n, re.S):
    for ref,pin in re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)',m.group(2)): nets[(ref,pin)]=m.group(1)
s=open(PCB).read()
# 1) Netznamen: "X" -> "/X"
names={v for v in nets.values() if v.startswith('/')}
for full in sorted(names,key=len,reverse=True):
    short=full[1:]
    s=s.replace('(net "%s")'%short,'(net "%s")'%full)
# Netzliste im Kopf der Datei (falls vorhanden)
def footprints(s):
    i=0
    while True:
        i=s.find('\n\t(footprint "',i)
        if i<0: return
        d=0;k=i+1
        while True:
            c=s[k]
            if c=='(': d+=1
            elif c==')':
                d-=1
                if d==0: break
            k+=1
        yield i+1,k+1
        i=k
out=[];last=0
for a,e in footprints(s):
    blk=s[a:e]
    ref=re.search(r'\(property "Reference" "([^"]+)"',blk).group(1)
    f=comps.get(ref,{})
    # 2) unbeschaltete Pads mit Netznamen aus dem Schaltplan
    def padfix(m):
        p=m.group(0); num=m.group(1)
        nm=nets.get((ref,num))
        if nm and '(net ' not in p:
            p=p.replace('\t\t\t(uuid','\t\t\t(net "%s")\n\t\t\t(uuid'%nm,1)
        elif nm:
            p=re.sub(r'\(net "[^"]*"\)','(net "%s")'%nm.replace('\\','\\\\'),p,count=1)   # Netz aus dem Schaltplan
        return p
    blk=re.sub(r'\(pad "([^"]*)" .*?\n\t\t\)',padfix,blk,flags=re.S)
    # 3) Felder Hersteller/MPN/Mouser + Beschreibung
    for key in ('Hersteller','MPN','Mouser'):
        if key in f and '(property "%s"'%key in blk:
            blk=re.sub(r'\(property "%s" "[^"]*"'%key, lambda m,k=key:'(property "%s" "%s"'%(k,f[k]), blk, count=1)
        if key in f and '(property "%s"'%key not in blk:
            prop='\t\t(property "%s" "%s"\n\t\t\t(at 0 0 0)\n\t\t\t(layer "F.Fab")\n\t\t\t(hide yes)\n\t\t\t(uuid "%s")\n\t\t\t(effects\n\t\t\t\t(font\n\t\t\t\t\t(size 1 1)\n\t\t\t\t\t(thickness 0.15)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n'%(key,f[key],uuid.uuid4())
            j=blk.find('\t\t(attr'); blk=blk[:j]+prop+blk[j:]
    if f.get('_value'):
        blk=re.sub(r'\(property "Value" "[^"]*"', lambda m:'(property "Value" "%s"'%f['_value'], blk, count=1)
    if f.get('Datasheet'):
        blk=re.sub(r'\(property "Datasheet" "[^"]*"','(property "Datasheet" "%s"'%f['Datasheet'],blk,count=1)
    if f.get('Description') is not None:
        blk=re.sub(r'\(property "Description" "[^"]*"','(property "Description" "%s"'%f['Description'].replace('"',"'"),blk,count=1)
    # 4) Attribute
    if ref=='J2': blk=re.sub(r'\(attr [^)]*\)','(attr through_hole dnp)',blk,count=1)
    if ref in ('TP1','TP2'): blk=re.sub(r'\(attr [^)]*\)','(attr through_hole exclude_from_bom)',blk,count=1)
    out.append(s[last:a]); out.append(blk); last=e
out.append(s[last:]); s=''.join(out)
open(PCB,'w').write(s)
# Netzklassen-Muster an die Netznamen des Schaltplans anpassen
p=json.load(open(PRO))
alle=set(nets.values())
for pat in p['net_settings'].get('netclass_patterns',[]):
    b=pat['pattern'].lstrip('/')
    pat['pattern']=b if b in alle else '/'+b   # Versorgungssymbole (GND, +5V, +3V3) ohne '/'
open(PRO,'w').write(json.dumps(p,indent=2))
print('ok', len(comps),'Bauteile', len(names),'Netze')
