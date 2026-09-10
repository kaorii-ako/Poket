"""Rebuild the Poket assembly animation scene in Blender.

Run inside Blender (Scripting tab, or via the Blender MCP). Expects the STLs
exported from cad/enclosure.py plus the two KiCad board renders in tex/.
Renders a PNG sequence to frames/; encode with:

    ffmpeg -framerate 30 -i frames/f_%04d.png -c:v libx264 -pix_fmt yuv420p \
           -crf 18 -preset slow ../video/poket-assembly.mp4
"""
import bpy, os, math, numpy as np
from mathutils import Matrix, Vector

SRC = os.path.dirname(os.path.abspath(bpy.data.filepath or __file__)) + "/"
TEX = SRC + "tex/"
BW, BH = 0.080, 0.054          # board size, metres

# ---------------------------------------------------------------- clean slate
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

# ------------------------------------------------------------------- geometry
parts = [("back-shell.stl", "BackShell"), ("front-shell.stl", "FrontShell"),
         ("slider-cap.stl", "SliderCap"), ("pcb.stl", "PCB")]
objs = []
for f, n in parts:
    try:    bpy.ops.wm.stl_import(filepath=SRC + f, global_scale=0.001)
    except AttributeError: bpy.ops.import_mesh.stl(filepath=SRC + f, global_scale=0.001)
    o = bpy.context.selected_objects[0]; o.name = n; objs.append(o)

# STL mesh data is in millimetres and the 0.001 lives in object scale:
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

# ------------------------------------------------------------------ materials
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

def plastic(name, rgb, rough):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*rgb, 1)
    b.inputs["Roughness"].default_value = rough
    return m
shell = plastic("Shell_PETG", (0.040, 0.045, 0.055), 0.50)
cap   = plastic("Cap_PETG",   (0.72, 0.26, 0.07),   0.45)
for n, m in (("BackShell", shell), ("FrontShell", shell), ("SliderCap", cap)):
    bpy.data.objects[n].data.materials.append(m)

# ---------------------------------------------------------------------- stage
w_ = bpy.context.scene.world or bpy.data.worlds.new("World")
bpy.context.scene.world = w_; w_.use_nodes = True
wn = w_.node_tree; wn.nodes.clear()
wout, bg = wn.nodes.new("ShaderNodeOutputWorld"), wn.nodes.new("ShaderNodeBackground")
bg.inputs["Color"].default_value = (0.021, 0.023, 0.028, 1)
wn.links.new(bg.outputs["Background"], wout.inputs["Surface"])
bpy.ops.mesh.primitive_plane_add(size=1.4, location=(0, 0, -0.0062))
bpy.context.object.name = "Floor"
bpy.context.object.data.materials.append(plastic("Floor_mat", (0.013, 0.014, 0.017), 0.62))

# area lights: an 8 cm object at 25 cm needs single-digit watts, not hundreds
def area(name, loc, rot, size, energy, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, type='AREA'); d.size, d.energy, d.color = size, energy, color
    o = bpy.data.objects.new(name, d); bpy.context.collection.objects.link(o)
    o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
area("Key",  (-0.22, -0.26, 0.32), ( 42, 0, -40), 0.45, 7.5)
area("Fill", ( 0.30, -0.18, 0.16), ( 66, 0,  58), 0.55, 2.5, (0.85, 0.90, 1.00))
area("Rim",  ( 0.10,  0.34, 0.24), (-58, 0,  14), 0.50, 6.0, (0.80, 0.88, 1.00))

cd = bpy.data.cameras.new("Cam"); cd.lens = 48
cam = bpy.data.objects.new("Camera", cd); bpy.context.collection.objects.link(cam)
cam.location = (0.168, -0.198, 0.142)
tgt = bpy.data.objects.new("CamTarget", None); bpy.context.collection.objects.link(tgt)
tgt.location = (0, 0, 0.004)
c = cam.constraints.new('TRACK_TO'); c.target = tgt
c.track_axis, c.up_axis = 'TRACK_NEGATIVE_Z', 'UP_Y'
bpy.context.scene.camera = cam

# ------------------------------------------------------------------ animation
sc = bpy.context.scene
sc.frame_start, sc.frame_end = 1, 194
def fcurves_of(obj):                      # Blender 4.4+ moved fcurves into action slots
    ad = obj.animation_data
    if not ad or not ad.action: return []
    if hasattr(ad.action, "fcurves"): return list(ad.action.fcurves)
    return [fc for layer in ad.action.layers for strip in layer.strips
            for fc in (strip.channelbag(ad.action_slot).fcurves if strip.channelbag(ad.action_slot) else [])]
def drop(obj, height, f0, f1):
    obj.location.z = height; obj.keyframe_insert("location", index=2, frame=f0)
    obj.location.z = 0.0;    obj.keyframe_insert("location", index=2, frame=f1)
    for fc in fcurves_of(obj):
        for kp in fc.keyframe_points:
            kp.interpolation = 'SINE'
            kp.easing = 'EASE_IN' if kp.co[0] == f0 else 'EASE_OUT'
D = bpy.data.objects
drop(D["PCB"],        0.038,  1, 40)      # board settles into the tray
drop(D["FrontShell"], 0.105, 20, 66)      # lid drops on
drop(D["SliderCap"],  0.150, 46, 84)      # slider drops into its slot
piv.keyframe_insert("rotation_euler", index=2, frame=1)
piv.keyframe_insert("rotation_euler", index=2, frame=96)
piv.rotation_euler.z = math.radians(360)
piv.keyframe_insert("rotation_euler", index=2, frame=194)
kps = fcurves_of(piv)[0].keyframe_points
kps[0].interpolation = 'CONSTANT'
kps[1].interpolation = 'SINE'; kps[1].easing = 'EASE_IN'
kps[2].interpolation = 'SINE'; kps[2].easing = 'EASE_OUT'

# ------------------------------------------------------------------- renderer
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.render.fps = 30
sc.view_settings.view_transform = 'AgX'
try: sc.view_settings.look = 'AgX - Medium High Contrast'
except TypeError: pass
for attr, val in (("taa_render_samples", 64), ("use_gtao", True), ("use_raytracing", True),
                  ("use_shadows", True), ("use_soft_shadows", True)):
    if hasattr(sc.eevee, attr):
        try: setattr(sc.eevee, attr, val)
        except Exception: pass
sc.render.image_settings.file_format = 'PNG'
sc.render.filepath = SRC + "frames/f_"
print("scene ready - render the animation to", sc.render.filepath)
