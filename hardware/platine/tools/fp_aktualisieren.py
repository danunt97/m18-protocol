"""Uebernimmt die Grafik (Siebdruck, Fab, Courtyard) der Projekt-Footprints aus lib/m18.pretty in die Platine,
wie 'Footprints aus Bibliothek aktualisieren' (KiCad-10-SWIG kann FootprintLoad nicht). Pads bleiben unveraendert.
Aufruf: python3 tools/fp_aktualisieren.py"""
import re, os, glob
HERE=os.path.dirname(os.path.abspath(__file__)); PRJ=os.path.dirname(HERE)
PCB=os.path.join(PRJ,'m18-bruecke.kicad_pcb')
GR=('fp_line','fp_arc','fp_circle','fp_rect','fp_poly')
def items(blk):
    """Grafik-Elemente auf Ebene 1 eines Footprint-Blocks (Text-Spannen)."""
    out=[];i=0
    while True:
        m=re.search(r'\n\t+\((%s)\n'%'|'.join(GR),blk[i:])
        if not m: return out
        a=i+m.start()+1; d=0; k=blk.index('(',a)
        while True:
            c=blk[k]
            if c=='(': d+=1
            elif c==')':
                d-=1
                if d==0: break
            k+=1
        out.append((a,k+1)); i=k+1
def blocks(s):
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
        yield i+1,k+1; i=k
lib={os.path.basename(f)[:-10]:open(f).read() for f in glob.glob(os.path.join(PRJ,'lib','m18.pretty','*.kicad_mod'))}
s=open(PCB).read(); out=[];last=0;n=0
for a,e in blocks(s):
    blk=s[a:e]; name=re.match(r'\t\(footprint "m18:([^"]+)"',blk)
    unten='\n\t\t(layer "B.Cu")' in blk[:200]   # gespiegelte Footprints (Unterseite) nicht anfassen
    if name and name.group(1) in lib and not unten:
        lb=lib[name.group(1)]
        neu=''.join('\t\t'+lb[x:y].replace('\n\t','\n\t\t')+'\n' for x,y in items(lb))
        sp=items(blk)
        if sp:
            ind=sp[0][0]
            for x,y in reversed(sp):
                j=y
                while j<len(blk) and blk[j] in ' \t': j+=1
                if j<len(blk) and blk[j]=='\n': j+=1
                blk=blk[:x]+blk[j:]
            # Einrueckung vor dem ersten Element entfernen und neue Grafik dort einsetzen
            k=blk.rfind('\n',0,ind)+1
            blk=blk[:k]+neu+blk[k:]
            n+=1
    out.append(s[last:a]); out.append(blk); last=e
out.append(s[last:]); open(PCB,'w').write(''.join(out)); print('aktualisiert',n,'Footprints')
