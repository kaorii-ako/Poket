import FreeCAD as App, Part, MeshPart
from FreeCAD import Vector
doc = App.openDocument("/var/home/hxshino/projects/Poket/cad/poket-enclosure.FCStd")
g = {o.Name: o for o in doc.Objects}
if "PCB" not in g:
    pcb = Part.Shape(); pcb.read("/var/home/hxshino/projects/Poket/enclosure/poket-pcb.step")
    pcb.translate(Vector(0, 54, 0))
    o = doc.addObject("Part::Feature", "PCB"); o.Shape = pcb; doc.recompute()
    g = {o.Name: o for o in doc.Objects}
back, front, cap, pcb = (g[n].Shape for n in ("BackShell", "FrontShell", "SliderCap", "PCB"))
for n, s in (("back shell", back), ("front shell", front), ("slider cap", cap)):
    print("FIT %-12s vs PCB = %.4f mm3" % (n, s.common(pcb).Volume))
print("FIT front vs back = %.4f mm3" % front.common(back).Volume)
print("FIT cap vs front  = %.4f mm3" % cap.common(front).Volume)
B = "/var/home/hxshino/projects/Poket/blender/"
for n, f in (("BackShell", "back-shell.stl"), ("FrontShell", "front-shell.stl"),
             ("SliderCap", "slider-cap.stl"), ("PCB", "pcb.stl")):
    m = MeshPart.meshFromShape(Shape=g[n].Shape, LinearDeflection=0.02,
                               AngularDeflection=0.2, Relative=False)
    m.write(B + f); print("MESH %-16s %6d tris" % (f, m.CountFacets))
doc.save()
print("DONE")
