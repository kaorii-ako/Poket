import os, Part
from FreeCAD import Vector
E = "/var/home/hxshino/projects/Poket/enclosure"
def load(n):
    s = Part.Shape(); s.read(os.path.join(E, n)); return s
pcb = load("poket-pcb.step")
pcb.translate(Vector(0, 54, 0))          # KiCad Y-down -> case Y-up
bb = pcb.BoundBox
print("PCB in case coords: x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f]" %
      (bb.XMin, bb.XMax, bb.YMin, bb.YMax, bb.ZMin, bb.ZMax))
for n in ("poket-back-shell.step", "poket-front-shell.step"):
    sh = load(n)
    c = sh.common(pcb)
    print("%-26s interference volume = %.4f mm3" % (n, c.Volume))
