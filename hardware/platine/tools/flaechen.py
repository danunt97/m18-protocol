"""GND-Flaechen auf beiden Lagen (nach Freerouting)."""
import pcbnew, os
HERE=os.path.dirname(os.path.abspath(__file__)); PCB=os.path.join(os.path.dirname(HERE),'m18-bruecke.kicad_pcb')
O=(100.0,100.0)
b=pcbnew.LoadBoard(PCB)
W=H=None
bb=b.GetBoardEdgesBoundingBox(); W=round(pcbnew.ToMM(bb.GetWidth())-0.1,3); H=round(pcbnew.ToMM(bb.GetHeight())-0.1,3)
OLDZ=[z for z in b.Zones() if not z.GetIsRuleArea()]  # erst am Ende entfernen (KiCad-10-SWIG)
NEW=[]
gnd=b.FindNet('GND')
for layer in (pcbnew.F_Cu,pcbnew.B_Cu):
    z=pcbnew.ZONE(b); z.SetLayer(layer); z.SetNet(gnd)
    ol=z.Outline(); ol.NewOutline()
    for x,y in ((0,0),(W,0),(W,H),(0,H)): ol.Append(pcbnew.FromMM(O[0]+x),pcbnew.FromMM(O[1]+y))
    z.SetLocalClearance(pcbnew.FromMM(0.3)); z.SetMinThickness(pcbnew.FromMM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(pcbnew.FromMM(0.4)); z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.5))
    z.SetAssignedPriority(0)
    b.Add(z); NEW.append(z)
# Sperrflaechen unter den Messern legt vorrouten.py an (vor Freerouting); hier bleiben sie stehen
for z in OLDZ: b.Remove(z)
pcbnew.ZONE_FILLER(b).Fill([z for z in NEW if not z.GetIsRuleArea()])
b.Save(PCB); print('ok',W,H)
