import sys, pcbnew
b = pcbnew.LoadBoard(sys.argv[1])
gnd = b.FindNet("GND").GetNetCode()
TI=[]
for t in b.GetTracks():
    if t.GetClass()=="PCB_VIA" and t.GetNetCode()==gnd: TI.append(t.GetPosition())
for fp in b.GetFootprints():
    for pad in fp.Pads():
        if pad.GetNetCode()==gnd and pad.GetAttribute()!=pcbnew.PAD_ATTRIB_SMD: TI.append(pad.GetPosition())
for z in b.Zones():
    if z.GetNetCode()!=gnd: continue
    for l in (pcbnew.F_Cu, pcbnew.B_Cu):
        if not z.IsOnLayer(l): continue
        polys=z.GetFilledPolysList(l)
        for i in range(polys.OutlineCount()):
            sub=polys.Subset(i,i+1)
            if any(sub.Contains(p,-1,0) for p in TI): continue
            bb=sub.BBox()
            names=[]
            for fp in b.GetFootprints():
                for pad in fp.Pads():
                    if pad.GetNetCode()!=gnd: continue
                    if not pad.IsOnLayer(l): continue
                    if sub.Contains(pad.GetPosition(),-1,0):
                        names.append(fp.GetReference()+"."+pad.GetNumber())
            print("ISLAND %s area %.2f bbox (%.2f,%.2f)-(%.2f,%.2f) pads=%s" % (
                "F.Cu" if l==pcbnew.F_Cu else "B.Cu", sub.Area()/1e12,
                pcbnew.ToMM(bb.GetLeft()),pcbnew.ToMM(bb.GetTop()),
                pcbnew.ToMM(bb.GetRight()),pcbnew.ToMM(bb.GetBottom()), names))
