import sys, math, pcbnew
p = sys.argv[1]
b = pcbnew.LoadBoard(p)
gnd = b.FindNet("GND").GetNetCode()
VIA_R = pcbnew.FromMM(0.20)
CLR   = pcbnew.FromMM(0.155)   # via edge -> other-net copper
GCLR  = pcbnew.FromMM(0.05)

def seg_dist(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    L2 = dx * dx + dy * dy
    if L2 == 0: return math.hypot(px - x1, py - y1)
    t = max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / L2))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))

def collect():
    segs, circs, boxes = [], [], []
    for t in b.GetTracks():
        isg = t.GetNetCode() == gnd
        if t.GetClass() == "PCB_VIA":
            circs.append((t.GetPosition().x, t.GetPosition().y, t.GetWidth() / 2.0, isg))
        else:
            s, e = t.GetStart(), t.GetEnd()
            segs.append((s.x, s.y, e.x, e.y, t.GetWidth() / 2.0, isg))
    for fp in b.GetFootprints():
        for pad in fp.Pads():
            boxes.append((pad.GetBoundingBox(), pad.GetNetCode() == gnd))
    return segs, circs, boxes

def ties():
    out = []
    for t in b.GetTracks():
        if t.GetClass() == "PCB_VIA" and t.GetNetCode() == gnd:
            out.append(t.GetPosition())
    for fp in b.GetFootprints():
        for pad in fp.Pads():
            if pad.GetNetCode() == gnd and pad.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
                out.append(pad.GetPosition())
    return out

added = 0
for rnd in range(6):
    SEG, CIRC, BOX = collect(); TI = ties()
    todo = []
    for z in b.Zones():
        if z.GetNetCode() != gnd: continue
        for l in (pcbnew.F_Cu, pcbnew.B_Cu):
            if not z.IsOnLayer(l): continue
            polys = z.GetFilledPolysList(l)
            for i in range(polys.OutlineCount()):
                sub = polys.Subset(i, i + 1)
                if any(sub.Contains(pt, -1, 0) for pt in TI): continue
                todo.append(sub)
    if not todo: break

    def ok(px, py):
        for x1, y1, x2, y2, hw, isg in SEG:
            need = VIA_R + hw + (GCLR if isg else CLR)
            if seg_dist(px, py, x1, y1, x2, y2) < need: return False
        for cx, cy, r, isg in CIRC:
            need = VIA_R + r + (GCLR if isg else CLR)
            if math.hypot(px - cx, py - cy) < need: return False
        for bb, isg in BOX:
            inf = int(VIA_R + (GCLR if isg else CLR))
            if (bb.GetLeft()-inf <= px <= bb.GetRight()+inf and
                bb.GetTop()-inf <= py <= bb.GetBottom()+inf): return False
        return True

    placed = 0
    for sub in todo:
        bb = sub.BBox(); step = pcbnew.FromMM(0.1); found = None
        y = bb.GetTop()
        while y <= bb.GetBottom() and not found:
            x = bb.GetLeft()
            while x <= bb.GetRight():
                pt = pcbnew.VECTOR2I(int(x), int(y))
                if sub.Contains(pt, -1, 0) and ok(x, y):
                    found = pt; break
                x += step
            y += step
        if found:
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(found)
            v.SetWidth(pcbnew.FromMM(0.4)); v.SetDrill(pcbnew.FromMM(0.2))
            v.SetNetCode(gnd); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            b.Add(v); added += 1; placed += 1
        else:
            print("no room: bbox (%.1f,%.1f)-(%.1f,%.1f) area %.2f" % (
                pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()),
                pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom()), sub.Area()/1e12))
    print("round", rnd, "islands", len(todo), "vias placed", placed)
    pcbnew.ZONE_FILLER(b).Fill(b.Zones()); b.BuildConnectivity()
    if placed == 0: break
print("total vias added:", added)
pcbnew.SaveBoard(p, b)
