import traceback
try:
    import FreeCAD, Part
    d=FreeCAD.newDocument("t")
    b=Part.makeBox(10,10,10)
    Part.show(b); d.recompute()
    Part.export([d.Objects[0]], "/var/home/hxshino/projects/Poket/cad/_test.step")
    print("OK")
except Exception:
    traceback.print_exc()
