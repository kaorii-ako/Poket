# Poket enclosure - parametric build script (FreeCAD 1.1)
# Coordinates: x,y match the PCB (FreeCAD y = 54 - KiCad y). z=0 is the PCB bottom face.
#
# All visible outer edges are 45 deg CHAMFERS, not fillets - straight bevels, no curves.
# Internal features (cavity, lip, rebate) are left square; nobody sees them and boolean
# ops on square internal corners are far more reliable.
import os, traceback
import FreeCAD as App
import Part
from FreeCAD import Vector

OUT = "/var/home/hxshino/projects/Poket/enclosure"
os.makedirs(OUT, exist_ok=True)

# ---------------- parameters ----------------
PCB_W, PCB_H, PCB_T = 80.0, 54.0, 1.6
CLR       = 1.0     # PCB edge -> inner wall
WALL      = 2.0
CH_CORNER = 1.2     # chamfer on the four outer vertical corners
CH_RIM    = 0.8     # chamfer on the outer top / bottom rims

Z_BOT_OUT   = -6.2   # outside of back face
Z_FLOOR     = -4.7   # inside of back face
Z_PCB0      = 0.0
Z_PCB1      = PCB_T
Z_SPLIT     = 2.6    # shell parting plane
Z_FRONT_IN  = 9.1    # inside of front face
Z_TOP_OUT   = 10.6   # outside of front face
LIP_C       = 0.25   # slip fit between the front lip and the back rebate

IN_X0, IN_Y0 = -CLR, -CLR
IN_X1, IN_Y1 = PCB_W + CLR, PCB_H + CLR
OU_X0, OU_Y0 = IN_X0 - WALL, IN_Y0 - WALL
OU_X1, OU_Y1 = IN_X1 + WALL, IN_Y1 + WALL

HOLES = [(5.0, 49.0), (75.0, 49.0), (5.0, 6.0), (75.0, 6.0)]  # M2, FreeCAD coords
BOSS_R, INSERT_R, INSERT_D = 2.7, 1.6, 4.5
POST_R, SCREW_R = 2.6, 1.25
HEAD_R, HEAD_D  = 2.15, 1.3


def vertical_edges(shape):
    return [e for e in shape.Edges
            if abs(e.Vertexes[0].Point.x - e.Vertexes[-1].Point.x) < 1e-7
            and abs(e.Vertexes[0].Point.y - e.Vertexes[-1].Point.y) < 1e-7]


def rbox(x0, y0, z0, dx, dy, dz, c=0.0):
    """Box, optionally chamfered on its four vertical corners."""
    b = Part.makeBox(dx, dy, dz, Vector(x0, y0, z0))
    if c > 0:
        b = b.makeChamfer(c, vertical_edges(b))
    return b


def chamfer_at_z(shape, z, size):
    edges = [e for e in shape.Edges if all(abs(v.Point.z - z) < 1e-6 for v in e.Vertexes)]
    return shape.makeChamfer(size, edges) if edges else shape


def cylz(x, y, z0, z1, r):
    return Part.makeCylinder(r, z1 - z0, Vector(x, y, z0))


def cyly(x, z, y0, y1, r):
    return Part.makeCylinder(r, y1 - y0, Vector(x, y0, z), Vector(0, 1, 0))


doc = App.newDocument("poket_enclosure")

# ---------------- cut features ----------------
cuts_back = [
    rbox(54.6, OU_Y0 - 1.0, 1.2, 10.8, 4.0, 4.1),                    # USB-C
    cyly(12.1, Z_PCB1 + 3.05, IN_Y1 - 1.0, OU_Y1 + 2.0, 3.3),        # 3.5 mm jack
    rbox(OU_X0 - 1.0, 17.0, -2.1, 5.0, 14.0, 2.25),                  # microSD slot
]
for x in (32.0, 38.0):                                                # BOOT / RESET pinholes
    cuts_back.append(cylz(x, 4.0, Z_BOT_OUT - 1.0, Z_FLOOR + 0.5, 0.9))
for (x, y) in HOLES:
    cuts_back.append(cylz(x, y, Z_BOT_OUT - 1.0, Z_PCB0 + 0.5, SCREW_R))
    cuts_back.append(cylz(x, y, Z_BOT_OUT - 0.5, Z_BOT_OUT + HEAD_D, HEAD_R))

cuts_front = [rbox(18.0, 29.5, Z_FRONT_IN - 0.6, 26.0, 15.0, 3.0)]   # OLED window
for x in (24.0, 33.0, 42.0):                                          # transport buttons
    cuts_front.append(cylz(x, 11.0, Z_FRONT_IN - 0.6, Z_TOP_OUT + 1.0, 2.75))
cuts_front.append(cylz(59.5, 18.3, Z_FRONT_IN - 0.6, Z_TOP_OUT + 1.0, 3.75))   # encoder shaft
for x in (44.0, 47.5):                                                # status LEDs
    cuts_front.append(cylz(x, 5.0, Z_FRONT_IN - 0.6, Z_TOP_OUT + 1.0, 0.9))
cuts_front.append(rbox(20.2, 1.8, Z_FRONT_IN - 0.6, 7.6, 3.4, 3.0))  # power slider slot
for i in range(5):                                                    # vent grille (debossed)
    cuts_front.append(rbox(48.0, 45.6 + i * 1.9, Z_TOP_OUT - 0.7, 24.0, 1.2, 2.0))

# ---------------- back shell ----------------
back = rbox(OU_X0, OU_Y0, Z_BOT_OUT, OU_X1 - OU_X0, OU_Y1 - OU_Y0,
            Z_SPLIT - Z_BOT_OUT, CH_CORNER)
back = chamfer_at_z(back, Z_BOT_OUT, CH_RIM)
back = back.cut(rbox(IN_X0, IN_Y0, Z_FLOOR, IN_X1 - IN_X0, IN_Y1 - IN_Y0,
                     Z_SPLIT - Z_FLOOR + 1.0))
back = back.cut(rbox(IN_X0 - LIP_C, IN_Y0 - LIP_C, Z_PCB1,          # lip rebate
                     (IN_X1 - IN_X0) + 2 * LIP_C, (IN_Y1 - IN_Y0) + 2 * LIP_C,
                     Z_SPLIT - Z_PCB1 + 0.5))
for (x, y) in HOLES:                                                  # PCB support posts
    back = back.fuse(cylz(x, y, Z_FLOOR, Z_PCB0, POST_R))
for r in (rbox(20.0, 16.0, Z_FLOOR, 1.5, 26.0, 2.7),                  # battery pocket ribs
          rbox(58.5, 16.0, Z_FLOOR, 1.5, 26.0, 2.7),
          rbox(21.5, 14.5, Z_FLOOR, 37.0, 1.5, 2.7),
          rbox(21.5, 42.0, Z_FLOOR, 37.0, 1.5, 2.7)):
    back = back.fuse(r)
back = back.removeSplitter()
for c in cuts_back:
    back = back.cut(c)

# ---------------- front shell ----------------
front = rbox(OU_X0, OU_Y0, Z_SPLIT, OU_X1 - OU_X0, OU_Y1 - OU_Y0,
             Z_TOP_OUT - Z_SPLIT, CH_CORNER)
front = chamfer_at_z(front, Z_TOP_OUT, CH_RIM)
front = front.cut(rbox(IN_X0, IN_Y0, Z_SPLIT - 1.0, IN_X1 - IN_X0, IN_Y1 - IN_Y0,
                       Z_FRONT_IN - Z_SPLIT + 1.0))
lip_o = rbox(IN_X0 - LIP_C, IN_Y0 - LIP_C, Z_PCB1 + 0.1,
             (IN_X1 - IN_X0) + 2 * LIP_C, (IN_Y1 - IN_Y0) + 2 * LIP_C,
             (Z_SPLIT + 1.0) - (Z_PCB1 + 0.1))
lip_i = rbox(-0.3, -0.3, Z_PCB1 - 0.5, PCB_W + 0.6, PCB_H + 0.6, 6.0)
front = front.fuse(lip_o.cut(lip_i))
for (x, y) in HOLES:                                                  # heat-set insert bosses
    front = front.fuse(cylz(x, y, Z_PCB1, Z_FRONT_IN, BOSS_R))
front = front.removeSplitter()
for (x, y) in HOLES:
    front = front.cut(cylz(x, y, Z_PCB1 - 0.5, Z_PCB1 + INSERT_D, INSERT_R))
for c in cuts_front:
    front = front.cut(c)

# ---------------- power-slider cap (third printed part) ----------------
# grips the SMD slide-switch actuator and pokes through the slot in the front face
CAP_Z0, CAP_Z1 = 2.6, Z_TOP_OUT + 0.8
cap = rbox(-3.0, -1.5, CAP_Z0, 6.0, 3.0, CAP_Z1 - CAP_Z0, 0.5)
cap = cap.cut(rbox(-1.3, -0.8, CAP_Z0 - 0.1, 2.6, 1.6, 3.1))          # actuator pocket
# thumb grooves bite in from both long sides only - cutting straight through
# would slice the cap into four loose pieces
for i in range(3):
    z = CAP_Z1 - 1.9 + i * 0.6
    cap = cap.cut(rbox(-3.2, -1.6, z, 6.4, 0.8, 0.3))
    cap = cap.cut(rbox(-3.2,  0.8, z, 6.4, 0.8, 0.3))
cap.translate(Vector(24.0, 3.5, 0.0))

# ---------------- PCB stand-in (reference only) ----------------
pcb = Part.makeBox(PCB_W, PCB_H, PCB_T)
pcb = pcb.makeFillet(4.0, vertical_edges(pcb))
for (x, y) in HOLES:
    pcb = pcb.cut(cylz(x, y, -1, PCB_T + 1, 1.1))

for name, shape in (("BackShell", back), ("FrontShell", front),
                    ("SliderCap", cap), ("PCB_ref", pcb)):
    o = doc.addObject("Part::Feature", name)
    o.Shape = shape
doc.recompute()

g = {o.Name: o for o in doc.Objects}
Part.export([g["BackShell"]],  os.path.join(OUT, "poket-back-shell.step"))
Part.export([g["FrontShell"]], os.path.join(OUT, "poket-front-shell.step"))
Part.export([g["SliderCap"]],  os.path.join(OUT, "poket-slider-cap.step"))
Part.export([g["BackShell"], g["FrontShell"], g["SliderCap"]],
            os.path.join(OUT, "poket-enclosure-assembly.step"))
import MeshPart
for n, f in (("BackShell", "poket-back-shell.stl"), ("FrontShell", "poket-front-shell.stl"),
             ("SliderCap", "poket-slider-cap.stl")):
    m = MeshPart.meshFromShape(Shape=g[n].Shape, LinearDeflection=0.02,
                               AngularDeflection=0.2, Relative=False)
    m.write(os.path.join(OUT, f))

doc.saveAs("/var/home/hxshino/projects/Poket/cad/poket-enclosure.FCStd")
print("VOL back=%.2f front=%.2f cap=%.3f cm3" % (back.Volume/1000, front.Volume/1000, cap.Volume/1000))
for n, sh in (("back", back), ("front", front), ("cap", cap)):
    print("%-6s solids=%d valid=%s" % (n, len(sh.Solids), sh.isValid()))
print("envelope %.1f x %.1f x %.1f mm" % (back.BoundBox.XLength, back.BoundBox.YLength,
                                          Z_TOP_OUT - Z_BOT_OUT))
print("DONE")
