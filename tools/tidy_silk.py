import sys, re, pcbnew
p = sys.argv[1]
b = pcbnew.LoadBoard(p)
moved = 0
for fp in b.GetFootprints():
    ref = fp.GetReference()
    txt = fp.Reference()
    # passives and diodes: reference belongs on Fab, there is no room on silk
    if re.match(r'^(R|C|FB|D|L|MH)\d', ref):
        lyr = pcbnew.F_Fab if fp.GetLayer() == pcbnew.F_Cu else pcbnew.B_Fab
        if txt.GetLayer() != lyr:
            txt.SetLayer(lyr); moved += 1
    val = fp.Value()
    if val.IsVisible():
        val.SetVisible(False)
print("refs moved to fab:", moved)
pcbnew.SaveBoard(p, b)
