import os, math, traceback
import FreeCAD as App, Part, TechDraw
from FreeCAD import Vector

SRC = "/var/home/hxshino/projects/Poket/enclosure"
OUT = "/var/home/hxshino/projects/Poket/cad/_svg"
os.makedirs(OUT, exist_ok=True)

def basis(d):
    d = d.normalize()
    up = Vector(0, 0, 1)
    if abs(d.dot(up)) > 0.95: up = Vector(0, 1, 0)
    u = d.cross(up); u.normalize()
    v = u.cross(d); v.normalize()
    return u, v

def render(shapes_colors, direction, path, W=1400, pad=30):
    d = Vector(*direction)
    u, v = basis(d)
    polys = []
    for shp, col, wdt in shapes_colors:
        res = TechDraw.projectEx(shp, d)
        vis = res[0]          # visible sharp edges
        for grp in (res[0], res[2], res[3]):
            if grp is None: continue
            for e in grp.Edges:
                pts = e.discretize(Number=max(2, int(e.Length * 2) + 2))
                polys.append(([(p.dot(u), p.dot(v)) for p in pts], col, wdt))
    xs = [p[0] for pl, _, _ in polys for p in pl]
    ys = [p[1] for pl, _, _ in polys for p in pl]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    s = (W - 2 * pad) / (x1 - x0)
    H = int((y1 - y0) * s + 2 * pad)
    def T(p): return (pad + (p[0] - x0) * s, H - pad - (p[1] - y0) * s)
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (W, H, W, H),
           '<rect width="100%%" height="100%%" fill="#f7f7f5"/>']
    for pl, col, wdt in polys:
        pts = " ".join("%.2f,%.2f" % T(p) for p in pl)
        out.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%s" stroke-linecap="round" stroke-linejoin="round"/>' % (pts, col, wdt))
    out.append('</svg>')
    open(path, 'w').write("\n".join(out))
    print("wrote", path)

def load(name):
    sh = Part.Shape(); sh.read(os.path.join(SRC, name)); return sh

back  = load("poket-back-shell.step")
front = load("poket-front-shell.step")
cap   = load("poket-slider-cap.step")

render([(front, "#1b2a3a", "1.1")], (-0.55, -0.62, -0.56), os.path.join(OUT, "front-iso.svg"))
render([(back, "#1b2a3a", "1.1")], (-0.55, -0.62, 0.56), os.path.join(OUT, "back-iso.svg"))
render([(front, "#1b2a3a", "1.1")], (0, 0, -1), os.path.join(OUT, "front-face.svg"))
asm = Part.makeCompound([back, front, cap])
render([(asm, "#1b2a3a", "1.0")], (-0.55, -0.62, -0.56), os.path.join(OUT, "assembly-iso.svg"))
print("DONE")
