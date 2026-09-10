import sys, pcbnew
p = sys.argv[1]
b = pcbnew.LoadBoard(p)
for z in b.Zones():
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)   # solid pad connection, no thermal spokes
    z.SetLocalClearance(pcbnew.FromMM(0.15))
    z.SetMinThickness(pcbnew.FromMM(0.2))
    try:
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)  # drop orphan copper islands
    except AttributeError:
        z.SetIslandRemovalMode(0)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.BuildConnectivity()
pcbnew.SaveBoard(p, b)
print("zones:", len(b.Zones()), "filled solid, islands removed")
