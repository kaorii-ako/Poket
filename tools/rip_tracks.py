import sys, pcbnew
p=sys.argv[1]; b=pcbnew.LoadBoard(p)
n=0
for t in list(b.GetTracks()):
    b.Remove(t); n+=1
b.BuildConnectivity(); pcbnew.SaveBoard(p,b)
print("removed", n, "track/via items")
