import struct, math, sys, os

def read_stl(path):
    with open(path,'rb') as f:
        head=f.read(84)
        n=struct.unpack('<I', head[80:84])[0]
        tris=[]
        for _ in range(n):
            d=f.read(50)
            if len(d)<50: break
            v=struct.unpack('<12fH', d)
            tris.append(((v[3],v[4],v[5]),(v[6],v[7],v[8]),(v[9],v[10],v[11])))
    return tris

def norm(a):
    l=math.sqrt(sum(c*c for c in a)) or 1.0
    return tuple(c/l for c in a)
def sub(a,b): return (a[0]-b[0],a[1]-b[1],a[2]-b[2])
def cross(a,b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def dot(a,b): return sum(x*y for x,y in zip(a,b))

def render(tri_sets, view, out, W=1300, pad=26, bg="#f2f1ee", light=(-0.35,-0.5,0.79)):
    d=norm(view)
    up=(0,0,1)
    if abs(dot(d,up))>0.95: up=(0,1,0)
    u=norm(cross(d,up)); v=norm(cross(u,d))
    L=norm(light)
    faces=[]
    for tris, base in tri_sets:
        for t in tris:
            nrm=norm(cross(sub(t[1],t[0]), sub(t[2],t[0])))
            if dot(nrm,d) > -0.0001:   # camera looks along d
                continue
            sh=max(0.0, dot(nrm,L))
            lum=0.30+0.70*(sh**0.85)
            col=tuple(min(255,int(c*lum)) for c in base)
            p=[(dot(pt,u), dot(pt,v)) for pt in t]
            z=sum(dot(pt,d) for pt in t)/3.0
            faces.append((z,p,col))
    faces.sort(key=lambda f: -f[0])
    xs=[q[0] for _,p,_ in faces for q in p]; ys=[q[1] for _,p,_ in faces for q in p]
    x0,x1,y0,y1=min(xs),max(xs),min(ys),max(ys)
    s=(W-2*pad)/(x1-x0); H=int((y1-y0)*s+2*pad)
    def T(q): return (pad+(q[0]-x0)*s, H-pad-(q[1]-y0)*s)
    o=['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" shape-rendering="crispEdges">'%(W,H,W,H),
       '<rect width="100%%" height="100%%" fill="%s"/>'%bg]
    for _,p,c in faces:
        pts=" ".join("%.2f,%.2f"%T(q) for q in p)
        col='#%02x%02x%02x'%c
        o.append('<polygon points="%s" fill="%s" stroke="%s" stroke-width="0.4"/>'%(pts,col,col))
    o.append('</svg>')
    open(out,'w').write("\n".join(o))
    print("wrote", out, len(faces), "faces")

E="/var/home/hxshino/projects/Poket/enclosure"
front=read_stl(E+"/poket-front-shell.stl")
back=read_stl(E+"/poket-back-shell.stl")
cap=read_stl(E+"/poket-slider-cap.stl")
GREY=(96,102,110); ACC=(214,132,74)
O="/var/home/hxshino/projects/Poket/images/"
render([(front,GREY),(cap,ACC)], (0.45,0.62,-0.64), O+"case-front-iso.svg")
render([(back,GREY)], (0.45,0.62,0.64), O+"case-back-inside.svg")
render([(back,GREY)], (0.45,0.62,-0.64), O+"case-back-iso.svg")
render([(front,GREY),(cap,ACC)], (0.0,0.0,-1.0), O+"case-front-face.svg")
render([(back,GREY),(front,(120,126,134)),(cap,ACC)], (-0.5,0.68,-0.54), O+"case-assembly.svg")
