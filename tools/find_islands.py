import sys, pcbnew
b = pcbnew.LoadBoard(sys.argv[1])
gnd = b.FindNet("GND").GetNetCode()
LN = {pcbnew.F_Cu:"F.Cu", pcbnew.In1_Cu:"In1.Cu", pcbnew.In2_Cu:"In2.Cu", pcbnew.B_Cu:"B.Cu"}
# collect vias / through pads that tie F/B to inner layers
ties = []
for t in b.GetTracks():
    if t.GetClass() == "PCB_VIA" and t.GetNetCode() == gnd:
        ties.append(t.GetPosition())
for fp in b.GetFootprints():
    for pad in fp.Pads():
        if pad.GetNetCode() == gnd and pad.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
            ties.append(pad.GetPosition())
print("gnd ties (vias + THT pads):", len(ties))
for z in b.Zones():
    if z.GetNetCode() != gnd: continue
    for l in (pcbnew.F_Cu, pcbnew.B_Cu):
        if not z.IsOnLayer(l): continue
        polys = z.GetFilledPolysList(l)
        n = polys.OutlineCount()
        for i in range(n):
            sub = polys.Subset(i, i+1)
            area = sub.Area() / 1e12  # mm^2  (nm^2 -> mm^2)
            has = any(sub.Contains(p, -1, 0) for p in ties)
            if not has:
                bb = sub.BBox()
                print("%-6s island %2d area %7.2f mm2  bbox (%.1f,%.1f)-(%.1f,%.1f)  NO TIE" % (
                    LN[l], i, area,
                    pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()),
                    pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom())))
