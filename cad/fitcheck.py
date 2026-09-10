import os, Part, FreeCAD
from FreeCAD import Vector
E = "/var/home/hxshino/projects/Poket/enclosure"
pcb = Part.Shape(); pcb.read(os.path.join(E, "poket-pcb.step"))
bb = pcb.BoundBox
print("PCB STEP bbox: x[%.2f %.2f] y[%.2f %.2f] z[%.2f %.2f]" % (
    bb.XMin, bb.XMax, bb.YMin, bb.YMax, bb.ZMin, bb.ZMax))
print("solids:", len(pcb.Solids))
