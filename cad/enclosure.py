# Poket enclosure - parametric build script (FreeCAD 1.1, headless)
# Coordinates: x,y match the PCB (FreeCAD y = 54 - KiCad y). z=0 is the PCB bottom face.
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
R_OUT     = 6.0
R_IN      = 4.0

Z_BOT_OUT   = -6.2   # outside of back face
Z_FLOOR     = -4.7   # inside of back face
Z_PCB0      = 0.0
Z_PCB1      = PCB_T
Z_SPLIT     = 2.6    # shell parting plane
Z_FRONT_IN  = 9.1    # inside of front face
Z_TOP_OUT   = 10.6   # outside of front face

IN_X0, IN_Y0 = -CLR, -CLR
IN_X1, IN_Y1 = PCB_W + CLR, PCB_H + CLR
OU_X0, OU_Y0 = IN_X0 - WALL, IN_Y0 - WALL
OU_X1, OU_Y1 = IN_X1 + WALL, IN_Y1 + WALL

HOLES = [(5.0, 49.0), (75.0, 49.0), (5.0, 6.0), (75.0, 6.0)]  # M2, FreeCAD coords
BOSS_R, INSERT_R, INSERT_D = 2.7, 1.6, 4.5
POST_R, SCREW_R = 2.6, 1.25
HEAD_R, HEAD_D  = 2.15, 1.3

def rbox(x0, y0, z0, dx, dy, dz, r=0.0):
    b = Part.makeBox(dx, dy, dz, Vector(x0, y0, z0))
    if r > 0:
        vert = [e for e in b.Edges
                if abs(e.Vertexes[0].Point.x - e.Vertexes[-1].Point.x) < 1e-7
                and abs(e.Vertexes[0].Point.y - e.Vertexes[-1].Point.y) < 1e-7]
        b = b.makeFillet(r, vert)
    return b

def cylz(x, y, z0, z1, r):
    return Part.makeCylinder(r, z1 - z0, Vector(x, y, z0))

def cyly(x, z, y0, y1, r):
    return Part.makeCylinder(r, y1 - y0, Vector(x, y0, z), Vector(0, 1, 0))

doc = App.newDocument("poket_enclosure")

# ---------------- cut features shared by both shells ----------------
cuts_back, cuts_front = [], []

# USB-C, bottom wall
cuts_back.append(rbox(54.6, OU_Y0 - 1.0, 1.2, 10.8, 4.0, 4.1, 0.8))
# 3.5 mm jack, top wall
cuts_back.append(cyly(12.1, Z_PCB1 + 3.05, IN_Y1 - 1.0, OU_Y1 + 2.0, 3.3))
# microSD slot, left wall (socket is on the underside of the PCB)
cuts_back.append(rbox(OU_X0 - 1.0, 17.0, -2.1, 5.0, 14.0, 2.25, 0.4))
# BOOT / RESET pin holes through the back face
for x in (32.0, 38.0):
    cuts_back.append(cylz(x, 4.0, Z_BOT_OUT - 1.0, Z_FLOOR + 0.5, 0.9))
# screw clearance + head counterbore through the back face
for (x, y) in HOLES:
    cuts_back.append(cylz(x, y, Z_BOT_OUT - 1.0, Z_PCB0 + 0.5, SCREW_R))
    cuts_back.append(cylz(x, y, Z_BOT_OUT - 0.5, Z_BOT_OUT + HEAD_D, HEAD_R))

# front face openings
cuts_front.append(rbox(18.0, 29.5, Z_FRONT_IN - 0.6, 26.0, 15.0, 3.0, 1.5))   # OLED window
for x in (24.0, 33.0, 42.0):                                                   # buttons
    cuts_front.append(cylz(x, 11.0, Z_FRONT_IN - 0.6, Z_TOP_OUT + 1.0, 2.75))
cuts_front.append(cylz(59.5, 18.3, Z_FRONT_IN - 0.6, Z_TOP_OUT + 1.0, 3.75))   # encoder shaft
for x in (44.0, 47.5):                                                         # status LEDs
    cuts_front.append(cylz(x, 5.0, Z_FRONT_IN - 0.6, Z_TOP_OUT + 1.0, 0.9))
cuts_front.append(rbox(20.2, 1.8, Z_FRONT_IN - 0.6, 7.6, 3.4, 3.0, 0.6))       # power slider slot
# decorative vent grille (debossed, not through)
for i in range(5):
    y = 45.6 + i * 1.9
    cuts_front.append(rbox(48.0, y, Z_TOP_OUT - 0.7, 24.0, 1.2, 2.0, 0.55))

# ---------------- back shell ----------------
back = rbox(OU_X0, OU_Y0, Z_BOT_OUT, OU_X1 - OU_X0, OU_Y1 - OU_Y0, Z_SPLIT - Z_BOT_OUT, R_OUT)
cavity = rbox(IN_X0, IN_Y0, Z_FLOOR, IN_X1 - IN_X0, IN_Y1 - IN_Y0, Z_SPLIT - Z_FLOOR + 1.0, R_IN)
back = back.cut(cavity)
# rebate for the front shell's registration lip (0.25 mm slip fit)
LIP_C = 0.25
back = back.cut(rbox(IN_X0 - LIP_C, IN_Y0 - LIP_C, Z_PCB1,
                     (IN_X1 - IN_X0) + 2 * LIP_C, (IN_Y1 - IN_Y0) + 2 * LIP_C,
                     Z_SPLIT - Z_PCB1 + 0.5, R_IN + LIP_C))
# PCB support posts
for (x, y) in HOLES:
    back = back.fuse(cylz(x, y, Z_FLOOR, Z_PCB0, POST_R))
# battery pocket ribs
ribs = [rbox(20.0, 16.0, Z_FLOOR, 1.5, 26.0, 2.7),
        rbox(58.5, 16.0, Z_FLOOR, 1.5, 26.0, 2.7),
        rbox(21.5, 14.5, Z_FLOOR, 37.0, 1.5, 2.7),
        rbox(21.5, 42.0, Z_FLOOR, 37.0, 1.5, 2.7)]
for r in ribs:
    back = back.fuse(r)
back = back.removeSplitter()
for c in cuts_back:
    back = back.cut(c)
# soften the outside bottom edge
try:
    bot = [e for e in back.Edges if all(abs(v.Point.z - Z_BOT_OUT) < 1e-6 for v in e.Vertexes)]
    back = back.makeFillet(1.0, bot)
except Exception:
    print("back bottom fillet skipped")

# ---------------- front shell ----------------
front = rbox(OU_X0, OU_Y0, Z_SPLIT, OU_X1 - OU_X0, OU_Y1 - OU_Y0, Z_TOP_OUT - Z_SPLIT, R_OUT)
_top = [e for e in front.Edges if all(abs(v.Point.z - Z_TOP_OUT) < 1e-6 for v in e.Vertexes)]
front = front.makeFillet(1.2, _top)
fcav = rbox(IN_X0, IN_Y0, Z_SPLIT - 1.0, IN_X1 - IN_X0, IN_Y1 - IN_Y0, Z_FRONT_IN - Z_SPLIT + 1.0, R_IN)
front = front.cut(fcav)
# registration lip that drops into the back shell
LIP_C = 0.25
lip_o = rbox(IN_X0 - LIP_C, IN_Y0 - LIP_C, Z_PCB1 + 0.1,
             (IN_X1 - IN_X0) + 2 * LIP_C, (IN_Y1 - IN_Y0) + 2 * LIP_C,
             (Z_SPLIT + 1.0) - (Z_PCB1 + 0.1), R_IN + LIP_C)
lip_i = rbox(-0.3, -0.3, Z_PCB1 - 0.5, PCB_W + 0.6, PCB_H + 0.6, 6.0, R_IN - 0.5)
front = front.fuse(lip_o.cut(lip_i))
# screw bosses with heat-set insert bores
for (x, y) in HOLES:
    front = front.fuse(cylz(x, y, Z_PCB1, Z_FRONT_IN, BOSS_R))
front = front.removeSplitter()
for (x, y) in HOLES:
    front = front.cut(cylz(x, y, Z_PCB1 - 0.5, Z_PCB1 + INSERT_D, INSERT_R))
for c in cuts_front:
    front = front.cut(c)

# ---------------- power-slider cap (third printed part) ----------------
# grips the SMD slide-switch actuator and pokes through the slot in the front face
CAP_Z0, CAP_Z1 = 2.6, Z_TOP_OUT + 0.8
cap = rbox(-3.0, -1.5, CAP_Z0, 6.0, 3.0, CAP_Z1 - CAP_Z0, 0.8)
cap = cap.cut(rbox(-1.3, -0.8, CAP_Z0 - 0.1, 2.6, 1.6, 3.1))          # actuator pocket
for i in range(3):                                                      # thumb grooves
    cap = cap.cut(rbox(-3.2, -1.6, CAP_Z1 - 1.9 + i * 0.6, 6.4, 3.2, 0.3))
cap.translate(Vector(24.0, 3.5, 0.0))

# ---------------- PCB stand-in (reference only) ----------------
pcb = rbox(0, 0, Z_PCB0, PCB_W, PCB_H, PCB_T, 4.0)
for (x, y) in HOLES:
    pcb = pcb.cut(cylz(x, y, -1, PCB_T + 1, 1.1))

for name, shape in (("BackShell", back), ("FrontShell", front), ("SliderCap", cap), ("PCB_ref", pcb)):
    o = doc.addObject("Part::Feature", name)
    o.Shape = shape
doc.recompute()

g = {o.Name: o for o in doc.Objects}
Part.export([g["BackShell"]],  os.path.join(OUT, "poket-back-shell.step"))
Part.export([g["FrontShell"]], os.path.join(OUT, "poket-front-shell.step"))
Part.export([g["SliderCap"]],  os.path.join(OUT, "poket-slider-cap.step"))
Part.export([g["BackShell"], g["FrontShell"], g["SliderCap"], g["PCB_ref"]],
            os.path.join(OUT, "poket-enclosure-assembly.step"))
import Mesh
for n, f in (("BackShell", "poket-back-shell.stl"), ("FrontShell", "poket-front-shell.stl"),
             ("SliderCap", "poket-slider-cap.stl")):
    Mesh.export([g[n]], os.path.join(OUT, f))

doc.saveAs("/var/home/hxshino/projects/Poket/cad/poket-enclosure.FCStd")
print("VOL back=%.1f front=%.1f cap=%.2f" % (back.Volume/1000, front.Volume/1000, cap.Volume/1000))
print("BBOX", back.BoundBox)
print("SOLIDS back=%d front=%d valid=%s/%s" % (len(back.Solids), len(front.Solids), back.isValid(), front.isValid()))
print("DONE")
