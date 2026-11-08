"""Uebernimmt geaenderte Netznamen aus dem Schaltplan in die Platine, ohne neu zu routen.
Netze werden ueber ihre Pads zugeordnet (gleiche Pads = gleiches Netz); die Namen werden in Pads, Leiterbahnen,
Vias und Flaechen der .kicad_pcb sowie in den Netzklassen-Mustern der .kicad_pro ersetzt.
Aufruf aus hardware/platine:  python3 tools/netze_umbenennen.py"""
import re, os, subprocess, tempfile, json
HERE=os.path.dirname(os.path.abspath(__file__)); PRJ=os.path.dirname(HERE)
PCB=os.path.join(PRJ,'m18-bruecke.kicad_pcb'); SCH=os.path.join(PRJ,'m18-bruecke.kicad_sch'); PRO=os.path.join(PRJ,'m18-bruecke.kicad_pro')
CLI='/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli'
net=os.path.join(tempfile.mkdtemp(),'n.net')
subprocess.run([CLI,'sch','export','netlist','-o',net,SCH],check=True,capture_output=True)
n=open(net).read()
neu={}
for m in re.finditer(r'\(net\s*\(code "\d+"\)\s*\(name "([^"]+)"\)(.*?)\n\t\t\)', n, re.S):
    neu[frozenset(re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)',m.group(2)))]=m.group(1)
s=open(PCB).read()
alt={}
for fm in re.finditer(r'\n\t\(footprint "[^"]+"(.*?)\n\t\)', s, re.S):
    ref=re.search(r'\(property "Reference" "([^"]+)"',fm.group(1)).group(1)
    for pm in re.finditer(r'\(pad "([^"]*)" .*?\n\t\t\)', fm.group(1), re.S):
        nm=re.search(r'\(net "([^"]*)"\)',pm.group(0))
        if nm and pm.group(1): alt.setdefault(nm.group(1),set()).add((ref,pm.group(1)))
ren={}
for name,nodes in alt.items():
    ziel=neu.get(frozenset(nodes))
    if ziel is None: print('kein Gegenstueck:',name,sorted(nodes)); continue
    if ziel!=name: ren[name]=ziel
# in einem Durchgang ersetzen (Namen koennen sich gegenseitig enthalten)
if ren:
    pat=re.compile(r'"(%s)"'%'|'.join(re.escape(k) for k in sorted(ren,key=len,reverse=True)))
    s=pat.sub(lambda m:'"%s"'%ren[m.group(1)],s)
    open(PCB,'w').write(s)
p=json.load(open(PRO))
for pt in p['net_settings'].get('netclass_patterns',[]):
    if pt['pattern'] in ren: pt['pattern']=ren[pt['pattern']]
open(PRO,'w').write(json.dumps(p,indent=2))
for a,b in sorted(ren.items()): print('  %s -> %s'%(a,b))
print('umbenannt', len(ren),'Netze')
