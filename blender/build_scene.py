"""Rebuild the Poket Blender scene from scratch.

Everything here is derived from files in the repo - the STLs exported by
cad/enclosure.py and the two KiCad board renders in tex/. Nothing depends on
the .blend, so if the .blend is lost just run this again.

    MODE = "assembly"   194 frames, 6.5 s : parts drop in, then a turntable
    MODE = "promo"      660 frames, 22 s  : 5-shot product film

Run inside Blender (Scripting tab, or via the Blender MCP), render the PNG
sequence, then stitch with ffmpeg - see tools/encode_video.sh.
"""
import bpy, os, math, numpy as np
from mathutils import Matrix, Vector

MODE = globals().get("MODE", "promo")   # "assembly" or "promo";
                                        # override by exec-ing with {"MODE": ...}
SRC  = "/var/home/hxshino/projects/Poket/blender/"
TEX  = SRC + "tex/"
BW, BH = 0.080, 0.054          # board size, metres
FPS = 30

# ============================================================== helpers
def fcurves_of(obj):
    """Blender 4.4+ moved fcurves out of Action and into action slots."""
    ad = getattr(obj, "animation_data", None)
    if not ad or not ad.action:
        return []
    if hasattr(ad.action, "fcurves"):
        return list(ad.action.fcurves)
    out = []
    for layer in ad.action.layers:
        for strip in layer.strips:
            cb = strip.channelbag(ad.action_slot)
            if cb:
                out.extend(cb.fcurves)
    return out


def setkey(obj, path, idx, frame, value, interp='SINE', easing='EASE_IN_OUT'):
    v = list(getattr(obj, path)); v[idx] = value; setattr(obj, path, v)
    obj.keyframe_insert(path, index=idx, frame=frame)
    for fc in fcurves_of(obj):
        if fc.data_path == path and fc.array_index == idx:
            for kp in fc.keyframe_points:
                if abs(kp.co[0] - frame) < 0.5:
                    kp.interpolation, kp.easing = interp, easing


def plastic(name, rgb, rough):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = rough
    return m


# ============================================================== clean slate
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.lights,
             bpy.data.cameras, bpy.data.images, bpy.data.actions):
    for b in list(coll):
        if b.name != "Render Result":
            try: coll.remove(b)
            except Exception: pass

# ============================================================== geometry
parts = [("back-shell.stl", "BackShell"), ("front-shell.stl", "FrontShell"),
         ("slider-cap.stl", "SliderCap"), ("pcb.stl", "PCB")]
objs = []
for f, n in parts:
    try:    bpy.ops.wm.stl_import(filepath=SRC + f, global_scale=0.001)
    except AttributeError: bpy.ops.import_mesh.stl(filepath=SRC + f, global_scale=0.001)
    o = bpy.context.selected_objects[0]; o.name = n; objs.append(o)

# STL mesh data lands in millimetres with the 0.001 living in object scale:
# recentre in mm, then bake the scale so everything downstream is metres
for o in objs:
    o.data.transform(Matrix.Translation(Vector((-39.96, -26.973, 0.0)))); o.data.update()
bpy.ops.object.select_all(action='DESELECT')
for o in objs: o.select_set(True)
bpy.context.view_layer.objects.active = objs[0]
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
# shade_smooth_by_angle needs the objects SELECTED, not merely active
bpy.ops.object.shade_smooth_by_angle(angle=math.radians(30))
bpy.ops.object.select_all(action='DESELECT')

piv = bpy.data.objects.new("Pivot", None)
bpy.context.collection.objects.link(piv)
for o in objs:
    o.parent = piv; o.matrix_parent_inverse = piv.matrix_world.inverted()

# ============================================================== PCB material
# the board has no 3D component models (KiCad's 3dmodels package isn't installed),
# so project the orthographic KiCad renders onto the slab in object space and
# pick top vs bottom from the sign of the surface normal
for key, f in (("top", "pcb-top.png"), ("bottom", "pcb-bottom.png")):
    bpy.data.images.load(TEX + f, check_existing=True).name = "pcb_" + key
w, h = bpy.data.images["pcb_top"].size
px = np.array(bpy.data.images["pcb_top"].pixels[:], dtype=np.float32).reshape(h, w, 4)
cols = np.where((px[:, :, 3] > 0.02).any(axis=0))[0]      # board silhouette in the render
U0, U1 = cols.min() / w, (cols.max() + 1) / w
VH = ((U1 - U0) * w / (BW / BH)) / h                       # derive V from the true aspect
V0, V1 = 0.5 - VH / 2, 0.5 + VH / 2

pcb = bpy.data.objects["PCB"]
mat = bpy.data.materials.new("PCB_mat"); mat.use_nodes = True
nt = mat.node_tree; nt.nodes.clear()
def N(t, x, y):
    n = nt.nodes.new(t); n.location = (x, y); return n
out, bsdf = N("ShaderNodeOutputMaterial", 900, 0), N("ShaderNodeBsdfPrincipled", 600, 0)
nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
bsdf.inputs["Roughness"].default_value = 0.42
texco = N("ShaderNodeTexCoord", -900, 0); texco.object = pcb
def branch(img, y, flip_x):
    m = N("ShaderNodeMapping", -650, y)
    m.inputs["Scale"].default_value = ((-1 if flip_x else 1) * (U1-U0)/BW, (V1-V0)/BH, 1.0)
    m.inputs["Location"].default_value = ((U0+U1)/2, (V0+V1)/2, 0.0)
    nt.links.new(texco.outputs["Object"], m.inputs["Vector"])
    t = N("ShaderNodeTexImage", -420, y); t.image = bpy.data.images[img]
    t.extension, t.interpolation = "EXTEND", "Cubic"
    nt.links.new(m.outputs["Vector"], t.inputs["Vector"]); return t
t_top, t_bot = branch("pcb_top", 250, False), branch("pcb_bottom", -250, True)
geo, sep = N("ShaderNodeNewGeometry", -900, -520), N("ShaderNodeSeparateXYZ", -700, -520)
nt.links.new(geo.outputs["Normal"], sep.inputs["Vector"])
gt = N("ShaderNodeMath", -520, -520); gt.operation = "GREATER_THAN"; gt.inputs[1].default_value = 0.0
nt.links.new(sep.outputs["Z"], gt.inputs[0])
mix = N("ShaderNodeMix", 200, 0); mix.data_type = "RGBA"
nt.links.new(gt.outputs[0], mix.inputs["Factor"])
nt.links.new(t_bot.outputs["Color"], mix.inputs[6])
nt.links.new(t_top.outputs["Color"], mix.inputs[7])
nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
pcb.data.materials.append(mat)

shell = plastic("Shell_PETG", (0.040, 0.045, 0.055), 0.50)
cap   = plastic("Cap_PETG",   (0.72, 0.26, 0.07),   0.45)
for n, m in (("BackShell", shell), ("FrontShell", shell), ("SliderCap", cap)):
    bpy.data.objects[n].data.materials.append(m)

# lit OLED behind the display window - the module itself isn't modelled.
# window is x 18..44, y 29.5..44.5 in case coords; scene is centred by (-39.96, -26.973)
bpy.ops.mesh.primitive_plane_add(size=1.0, location=(-0.00896, 0.01003, 0.0084))
scr = bpy.context.object; scr.name = "Screen"
scr.scale = (0.0255, 0.0145, 1.0)
bpy.ops.object.transform_apply(scale=True)
sm = bpy.data.materials.new("Screen_mat"); sm.use_nodes = True
snt = sm.node_tree; snt.nodes.clear()
so = snt.nodes.new("ShaderNodeOutputMaterial"); so.location = (400, 0)
em = snt.nodes.new("ShaderNodeEmission"); em.location = (150, 0)
em.inputs["Color"].default_value = (0.45, 0.85, 1.0, 1)
em.inputs["Strength"].default_value = 2.4
snt.links.new(em.outputs["Emission"], so.inputs["Surface"])
scr.data.materials.append(sm)
scr.parent = piv; scr.matrix_parent_inverse = piv.matrix_world.inverted()

# ============================================================== stage
w_ = bpy.context.scene.world or bpy.data.worlds.new("World")
bpy.context.scene.world = w_; w_.use_nodes = True
wn = w_.node_tree; wn.nodes.clear()
wout, bg = wn.nodes.new("ShaderNodeOutputWorld"), wn.nodes.new("ShaderNodeBackground")
bg.inputs["Color"].default_value = (0.021, 0.023, 0.028, 1)
wn.links.new(bg.outputs["Background"], wout.inputs["Surface"])

# floor has to be big or its far edge draws a horizon line through the wide shots
bpy.ops.mesh.primitive_plane_add(size=1.4, location=(0, 0, -0.0062))
floor = bpy.context.object; floor.name = "Floor"; floor.scale = (6.0, 6.0, 1.0)
floor.data.materials.append(plastic("Floor_mat", (0.013, 0.014, 0.017), 0.62))

# area lights: an 8 cm object at 25 cm needs single-digit watts, not hundreds
def area(name, loc, rot, size, energy, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, type='AREA'); d.size, d.energy, d.color = size, energy, color
    o = bpy.data.objects.new(name, d); bpy.context.collection.objects.link(o)
    o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
area("Key",  (-0.22, -0.26, 0.32), ( 42, 0, -40), 0.45, 7.5)
area("Fill", ( 0.30, -0.18, 0.16), ( 66, 0,  58), 0.55, 2.5, (0.85, 0.90, 1.00))
area("Rim",  ( 0.10,  0.34, 0.24), (-58, 0,  14), 0.50, 6.0, (0.80, 0.88, 1.00))

tgt = bpy.data.objects.new("CamTarget", None)
bpy.context.collection.objects.link(tgt); tgt.location = (0, 0, 0.004)

def make_cam(name, lens, loc, rig=None):
    cd = bpy.data.cameras.new(name); cd.lens = lens
    c = bpy.data.objects.new(name, cd); bpy.context.collection.objects.link(c)
    c.location = loc
    if rig: c.parent = rig
    tr = c.constraints.new('TRACK_TO'); tr.target = tgt
    tr.track_axis, tr.up_axis = 'TRACK_NEGATIVE_Z', 'UP_Y'
    return c

def make_rig(name):
    e = bpy.data.objects.new(name, None); bpy.context.collection.objects.link(e); return e

sc = bpy.context.scene
D = bpy.data.objects

# ============================================================== animation
def drop(obj, height, f0, f1):
    setkey(obj, "location", 2, f0, height, interp='SINE', easing='EASE_IN')
    setkey(obj, "location", 2, f1, 0.0,    interp='SINE', easing='EASE_OUT')

if MODE == "assembly":
    sc.frame_start, sc.frame_end = 1, 194
    cam = make_cam("Camera", 48, (0.168, -0.198, 0.142))
    sc.camera = cam
    drop(D["PCB"],        0.038,  1, 40)
    drop(D["FrontShell"], 0.105, 20, 66)
    drop(D["SliderCap"],  0.150, 46, 84)
    setkey(piv, "rotation_euler", 2, 1,   0.0, interp='CONSTANT')
    setkey(piv, "rotation_euler", 2, 96,  0.0, interp='SINE', easing='EASE_IN')
    setkey(piv, "rotation_euler", 2, 194, math.radians(360), interp='SINE', easing='EASE_OUT')
    outdir = SRC + "frames/f_"

else:  # promo - five shots, cut with camera-bound timeline markers
    sc.frame_start, sc.frame_end = 1, 660
    rig1 = make_rig("Rig1"); cam1 = make_cam("Cam_Hero", 52, (0.132, -0.148, 0.040), rig1)
    cam2 = make_cam("Cam_Explode", 48, (0.150, -0.177, 0.128))
    cam3 = make_cam("Cam_Top",     55, (0.008, -0.055, 0.235))
    rig4 = make_rig("Rig4"); cam4 = make_cam("Cam_Side", 55, (0.0, -0.158, 0.058), rig4)
    cam5 = make_cam("Cam_Spin",    50, (0.135, -0.160, 0.100))

    setkey(rig1, "rotation_euler", 2, 1,   math.radians(-26))     # 1: slow low orbit
    setkey(rig1, "rotation_euler", 2, 120, math.radians( 14))
    for idx, a, b in ((0, 0.150, 0.132), (1, -0.177, -0.156), (2, 0.128, 0.116)):
        setkey(cam2, "location", idx, 121, a); setkey(cam2, "location", idx, 260, b)
    for idx, a, b in ((0, 0.008, 0.005), (1, -0.055, -0.032), (2, 0.235, 0.165)):
        setkey(cam3, "location", idx, 261, a); setkey(cam3, "location", idx, 380, b)
    setkey(rig4, "rotation_euler", 2, 381, math.radians(150))     # 4: low pass, port side
    setkey(rig4, "rotation_euler", 2, 500, math.radians(206))

    for mk in list(sc.timeline_markers): sc.timeline_markers.remove(mk)
    for f, c in ((1, cam1), (121, cam2), (261, cam3), (381, cam4), (501, cam5)):
        sc.timeline_markers.new("shot_%d" % f, frame=f).camera = c
    sc.camera = cam1

    # shot 1 shows it closed; the cut at 121 hides the jump back to exploded
    def staged(obj, height, f_go, f_land):
        setkey(obj, "location", 2, 1,      0.0,    interp='CONSTANT')
        setkey(obj, "location", 2, 120,    0.0,    interp='CONSTANT')
        setkey(obj, "location", 2, 121,    height, interp='CONSTANT')
        setkey(obj, "location", 2, f_go,   height, interp='SINE', easing='EASE_IN')
        setkey(obj, "location", 2, f_land, 0.0,    interp='SINE', easing='EASE_OUT')
    staged(D["PCB"],        0.038, 121, 172)
    staged(D["FrontShell"], 0.105, 145, 216)
    staged(D["SliderCap"],  0.150, 178, 246)

    setkey(piv, "rotation_euler", 2, 1,   0.0, interp='CONSTANT')  # 5: turntable
    setkey(piv, "rotation_euler", 2, 501, 0.0, interp='SINE', easing='EASE_IN')
    setkey(piv, "rotation_euler", 2, 660, math.radians(360), interp='SINE', easing='EASE_OUT')

    # screen is lit in the hero shot, dark while the board is out, back on with the lid
    strength = em.inputs["Strength"]
    for f, v in ((1, 2.4), (120, 2.4), (121, 0.0), (216, 0.0), (240, 2.4)):
        strength.default_value = v
        strength.keyframe_insert("default_value", frame=f)
    for fc in fcurves_of(snt):
        for kp in fc.keyframe_points:
            kp.interpolation = 'CONSTANT' if kp.co[0] <= 121 else 'SINE'
            kp.easing = 'EASE_IN_OUT'
    outdir = SRC + "promo/p_"

# ============================================================== renderer
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.render.fps = FPS
sc.view_settings.view_transform = 'AgX'
try: sc.view_settings.look = 'AgX - Medium High Contrast'
except TypeError: pass
for attr, val in (("taa_render_samples", 64), ("use_gtao", True), ("use_raytracing", True),
                  ("use_shadows", True), ("use_soft_shadows", True)):
    if hasattr(sc.eevee, attr):
        try: setattr(sc.eevee, attr, val)
        except Exception: pass
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGB'
sc.render.filepath = outdir
os.makedirs(os.path.dirname(outdir), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=SRC + "poket.blend")
print("MODE=%s  frames %d-%d (%.1fs)  ->  %s"
      % (MODE, sc.frame_start, sc.frame_end,
         (sc.frame_end - sc.frame_start + 1) / FPS, outdir))
