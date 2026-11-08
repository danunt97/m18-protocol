"""GND-Flaechen auf beiden Lagen (nach Freerouting)."""
import pcbnew, os
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
b=pcbnew.LoadBoard(PCB)
POSY={r:pcbnew.ToMM(b.FindFootprintByReference(r).GetPosition().y)-O[1] for r in ('X1','X2','X3','X4')}
W=H=None
bb=b.GetBoardEdgesBoundingBox(); W=round(pcbnew.ToMM(bb.GetWidth())-0.1,3); H=round(pcbnew.ToMM(bb.GetHeight())-0.1,3)
OLDZ=list(b.Zones())  # erst am Ende entfernen (KiCad-10-SWIG)
NEW=[]
gnd=b.FindNet('/GND')
for layer in (pcbnew.F_Cu,pcbnew.B_Cu):
    z=pcbnew.ZONE(b); z.SetLayer(layer); z.SetNet(gnd)
    ol=z.Outline(); ol.NewOutline()
    for x,y in ((0,0),(W,0),(W,H),(0,H)): ol.Append(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+y))
    z.SetLocalClearance(pcbnew.FromMM(0.3)); z.SetMinThickness(pcbnew.FromMM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(pcbnew.FromMM(0.4)); z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.5))
    z.SetAssignedPriority(0)
    b.Add(z); NEW.append(z)
# Unter den Messern (Unterseite) keine Kupferfuellung: Messer liegen sonst nur auf dem Loetstopplack
for ref in ('X1','X2','X3','X4'):
    y=POSY[ref]
    k=pcbnew.ZONE(b); k.SetIsRuleArea(True); k.SetLayer(pcbnew.B_Cu)
    k.SetDoNotAllowZoneFills(True); k.SetDoNotAllowTracks(False); k.SetDoNotAllowVias(False); k.SetDoNotAllowPads(False); k.SetDoNotAllowFootprints(False)
    ol=k.Outline(); ol.NewOutline()
    for x,yy in ((0,y-1.4),(18.8,y-1.4),(18.8,y+1.4),(0,y+1.4)): ol.Append(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+yy))
    k.SetZoneName('Messer_'+ref); b.Add(k); NEW.append(k)
for z in OLDZ: b.Remove(z)
pcbnew.ZONE_FILLER(b).Fill([z for z in NEW if not z.GetIsRuleArea()])
b.Save(PCB); print('ok',W,H)
