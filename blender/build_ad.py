"""Poket product-ad scene: floating unit, blue sky gradient, macro shots.

Styled after a phone-launch film - deep blue vertical gradient, product hanging
in space with no floor, long lenses and shallow depth of field, cut between a
wide establishing shot and macro details.

Run inside Blender, render the PNG sequence, then encode:
    ffmpeg -framerate 30 -i blender/ad/a_%04d.png -c:v libx264 \
           -pix_fmt yuv420p -crf 18 -preset slow video/poket-ad.mp4

There is also blender/build_scene.py for the assembly clip and the 5-shot promo;
this file is a separate look, not a mode of that one.
"""
import bpy, math, os

SRC = "/var/home/hxshino/projects/Poket/blender/"
FPS, END = 30, 720          # 24 s

# ------------------------------------------------------------------ geometry
# reuse the shared builder for the parts and materials, then throw away its stage
base = SRC + "build_scene.py"
exec(compile(open(base).read(), base, 'exec'), {"__name__": "__main__", "MODE": "assembly"})

sc = bpy.context.scene
D  = bpy.data.objects
for n in ("Floor", "Key", "Fill", "Rim", "Camera", "CamTarget"):
    if n in D: bpy.data.objects.remove(D[n], do_unlink=True)

unit = D["Pivot"]; unit.name = "Poket"
for n in ("PCB", "FrontShell", "SliderCap", "BackShell", "Screen", "Poket"):
    if n in D:
        D[n].animation_data_clear()
        D[n].location = (0, 0, 0); D[n].rotation_euler = (0, 0, 0)

# ------------------------------------------------------------------ look
sh = bpy.data.materials["Shell_PETG"].node_tree.nodes["Principled BSDF"]
sh.inputs["Base Color"].default_value = (0.013, 0.015, 0.020, 1)
sh.inputs["Roughness"].default_value = 0.27
for k, v in (("Coat Weight", 0.22), ("Specular IOR Level", 0.45)):
    if k in sh.inputs: sh.inputs[k].default_value = v
cp = bpy.data.materials["Cap_PETG"].node_tree.nodes["Principled BSDF"]
cp.inputs["Base Color"].default_value = (0.62, 0.20, 0.045, 1)
cp.inputs["Roughness"].default_value = 0.25

# vertical screen-space gradient. Driving a Gradient texture through a rotated
# Mapping node rendered flat; separating the Window coordinate is reliable.
w = sc.world; w.use_nodes = True
wn = w.node_tree; wn.nodes.clear()
wout = wn.nodes.new("ShaderNodeOutputWorld"); wout.location = (700, 0)
bg   = wn.nodes.new("ShaderNodeBackground");  bg.location = (500, 0)
ramp = wn.nodes.new("ShaderNodeValToRGB");    ramp.location = (250, 0)
sep  = wn.nodes.new("ShaderNodeSeparateXYZ"); sep.location = (50, 0)
tc   = wn.nodes.new("ShaderNodeTexCoord");    tc.location = (-160, 0)
wn.links.new(tc.outputs["Window"], sep.inputs["Vector"])
wn.links.new(sep.outputs["Y"], ramp.inputs["Fac"])
wn.links.new(ramp.outputs["Color"], bg.inputs["Color"])
wn.links.new(bg.outputs["Background"], wout.inputs["Surface"])
cr = ramp.color_ramp
cr.elements[0].position = 0.0; cr.elements[0].color = (0.50, 0.68, 0.88, 1)
cr.elements[1].position = 1.0; cr.elements[1].color = (0.006, 0.035, 0.175, 1)
cr.elements.new(0.45).color = (0.055, 0.190, 0.480, 1)
bg.inputs["Strength"].default_value = 1.1

def area(name, loc, rot, size, energy, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, type='AREA'); d.size, d.energy, d.color = size, energy, color
    o = bpy.data.objects.new(name, d); sc.collection.objects.link(o)
    o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
area("Key",  (-0.16, -0.26, 0.30), ( 30, 0, -26), 0.16, 3.2)
area("Top",  ( 0.02,  0.02, 0.34), (  0, 0,   0), 0.35, 1.6)
area("RimL", (-0.30,  0.20, 0.02), ( 74, 0, -128), 0.28, 7.0, (0.72, 0.83, 1.0))
area("RimR", ( 0.30,  0.22, 0.06), ( 72, 0,  126), 0.28, 8.5, (0.80, 0.88, 1.0))

# ------------------------------------------------------------------ motion
def fcurves_of(o):
    ad = o.animation_data
    if not ad or not ad.action: return []
    if hasattr(ad.action, "fcurves"): return list(ad.action.fcurves)
    return [fc for l in ad.action.layers for s in l.strips
            for fc in (s.channelbag(ad.action_slot).fcurves if s.channelbag(ad.action_slot) else [])]

def setkey(o, path, idx, frame, value, interp='SINE'):
    v = list(getattr(o, path)); v[idx] = value; setattr(o, path, v)
    o.keyframe_insert(path, index=idx, frame=frame)
    for fc in fcurves_of(o):
        if fc.data_path == path and fc.array_index == idx:
            for kp in fc.keyframe_points:
                if abs(kp.co[0] - frame) < 0.5:
                    kp.interpolation, kp.easing = interp, 'EASE_IN_OUT'

sc.frame_start, sc.frame_end = 1, END
unit.rotation_euler = (math.radians(18), math.radians(-8), math.radians(-40))
unit.keyframe_insert("rotation_euler", frame=1)
unit.rotation_euler = (math.radians(18), math.radians(-8), math.radians(310))
unit.keyframe_insert("rotation_euler", frame=END)
for fc in fcurves_of(unit):
    for kp in fc.keyframe_points: kp.interpolation = 'LINEAR'
for f, z in ((1, 0.0), (240, 0.004), (480, -0.003), (END, 0.0)):
    setkey(unit, "location", 2, f, z)

# ------------------------------------------------------------------ cameras
def cam(name, lens, fstop, loc, look):
    cd = bpy.data.cameras.new(name); cd.lens = lens
    cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    c = bpy.data.objects.new(name, cd); sc.collection.objects.link(c); c.location = loc
    t = bpy.data.objects.new(name.replace("Cam", "Tgt"), None)
    sc.collection.objects.link(t); t.location = look
    tr = c.constraints.new('TRACK_TO'); tr.target = t
    tr.track_axis, tr.up_axis = 'TRACK_NEGATIVE_Z', 'UP_Y'
    cd.dof.focus_object = t
    return c

# f/2 at 10 cm is all bokeh and no product - macro shots need f/6 and distance
c1 = cam("AdCam1", 70,  5.6, ( 0.100, -0.620, 0.160), (0.000,  0.000, 0.000))
c2 = cam("AdCam2", 100, 6.0, ( 0.055, -0.155, 0.075), (0.010, -0.006, 0.010))
c3 = cam("AdCam3", 90,  7.0, (-0.030, -0.205, 0.030), (0.004, -0.030, 0.005))
c4 = cam("AdCam4", 85,  4.0, ( 0.160, -0.270, 0.135), (0.000,  0.000, 0.004))
c5 = cam("AdCam5", 100, 6.0, ( 0.060, -0.140, 0.085), (0.022,  0.012, 0.012))
c6 = cam("AdCam6", 80,  5.0, ( 0.140, -0.300, 0.115), (0.000,  0.000, 0.002))

setkey(c1, "location", 1,   1, -0.620); setkey(c1, "location", 1, 120, -0.500)
setkey(c2, "location", 0, 121,  0.075); setkey(c2, "location", 0, 230,  0.020)
setkey(c3, "location", 0, 231, -0.055); setkey(c3, "location", 0, 340,  0.045)
setkey(c4, "location", 2, 341,  0.155); setkey(c4, "location", 2, 460,  0.105)
setkey(c5, "location", 1, 461, -0.140); setkey(c5, "location", 1, 570, -0.112)
for idx, a, b in ((0, 0.14, 0.20), (1, -0.30, -0.44), (2, 0.115, 0.155)):
    setkey(c6, "location", idx, 571, a); setkey(c6, "location", idx, END, b)

for mk in list(sc.timeline_markers): sc.timeline_markers.remove(mk)
for f, c in ((1, c1), (121, c2), (231, c3), (341, c4), (461, c5), (571, c6)):
    sc.timeline_markers.new("s%d" % f, frame=f).camera = c
sc.camera = c1

# ------------------------------------------------------------------ renderer
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.render.fps = FPS
sc.view_settings.view_transform = 'AgX'
try: sc.view_settings.look = 'AgX - Medium High Contrast'
except TypeError: pass
for attr, val in (("taa_render_samples", 96), ("use_raytracing", True), ("use_gtao", True),
                  ("use_shadows", True), ("use_soft_shadows", True)):
    if hasattr(sc.eevee, attr):
        try: setattr(sc.eevee, attr, val)
        except Exception: pass
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGB'
os.makedirs(SRC + "ad", exist_ok=True)
sc.render.filepath = SRC + "ad/a_"
bpy.ops.wm.save_as_mainfile(filepath=SRC + "poket-ad.blend")
print("ad scene ready: %d frames (%.0f s) -> %s" % (END, END / FPS, sc.render.filepath))
