import sys, pcbnew
p = sys.argv[1]
b = pcbnew.LoadBoard(p)
n_mirror = 0
for d in list(b.GetDrawings()):
    if d.GetClass() == "PCB_TEXT" and d.GetLayer() in (pcbnew.B_SilkS, pcbnew.B_Mask, pcbnew.B_Fab):
        if not d.IsMirrored():
            d.SetMirrored(True); n_mirror += 1
TO_FAB = {"Q1","U2","U3","SW1","SW2","SW3","SW4","SW5","SW6","J1"}
n_ref = 0
for fp in b.GetFootprints():
    if fp.GetReference() in TO_FAB:
        t = fp.Reference()
        lyr = pcbnew.F_Fab if fp.GetLayer() == pcbnew.F_Cu else pcbnew.B_Fab
        if t.GetLayer() != lyr:
            t.SetLayer(lyr); n_ref += 1
print("mirrored:", n_mirror, "refs->fab:", n_ref)
pcbnew.SaveBoard(p, b)
