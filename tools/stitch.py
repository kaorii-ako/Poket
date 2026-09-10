import sys, math, pcbnew
p = sys.argv[1]
b = pcbnew.LoadBoard(p)
gnd = b.FindNet("GND")
GNDCODE = gnd.GetNetCode()

# fill first so we can test zone membership
for z in b.Zones():
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetLocalClearance(pcbnew.FromMM(0.15))
    z.SetMinThickness(pcbnew.FromMM(0.2))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())

LAYERS = [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
zones = {}
for z in b.Zones():
    if z.GetNetCode() != GNDCODE: continue
    for l in LAYERS:
        if z.IsOnLayer(l):
            zones.setdefault(l, []).append(z)

CLR = pcbnew.FromMM(0.35)          # keep-away from any non-GND copper
VIA_R = pcbnew.FromMM(0.25)
obstacles = []                      # (bbox, is_gnd)
for t in b.GetTracks():
    obstacles.append((t.GetBoundingBox(), t.GetNetCode() == GNDCODE))
for fp in b.GetFootprints():
    for pad in fp.Pads():
        obstacles.append((pad.GetBoundingBox(), pad.GetNetCode() == GNDCODE))

def clear_of_obstacles(pt):
    for bb, is_gnd in obstacles:
        margin = CLR if not is_gnd else pcbnew.FromMM(0.2)
        inf = int(VIA_R + margin)
        if (bb.GetLeft() - inf <= pt.x <= bb.GetRight() + inf and
            bb.GetTop() - inf <= pt.y <= bb.GetBottom() + inf):
            return False
    return True

def in_gnd_zone(pt, layer):
    for z in zones.get(layer, []):
        poly = z.GetFilledPolysList(layer)
        if poly.Contains(pt, -1, pcbnew.FromMM(0.35)):
            return True
    return False

placed = 0
step = pcbnew.FromMM(float(sys.argv[2]) if len(sys.argv) > 2 else 3.0)
x0, y0 = pcbnew.FromMM(1.2), pcbnew.FromMM(1.2)
x1, y1 = pcbnew.FromMM(78.8), pcbnew.FromMM(52.8)
new = []
x = x0
while x <= x1:
    y = y0
    while y <= y1:
        pt = pcbnew.VECTOR2I(int(x), int(y))
        if in_gnd_zone(pt, pcbnew.F_Cu) and in_gnd_zone(pt, pcbnew.In1_Cu) and clear_of_obstacles(pt):
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(pt)
            v.SetWidth(pcbnew.FromMM(0.5)); v.SetDrill(pcbnew.FromMM(0.25))
            v.SetNetCode(GNDCODE)
            v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            b.Add(v); new.append(v); placed += 1
        y += step
    x += step
print("stitching vias placed:", placed)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.BuildConnectivity()
pcbnew.SaveBoard(p, b)
