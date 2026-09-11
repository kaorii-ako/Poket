"""Poket hero still - two translucent units floating, Cycles on the GPU.

Styled after a smoked-plastic cassette product shot: dark navy gradient, both
units hanging in the air, one flat and one tilted above it, soft reflection
below. The wordmark is booleaned INTO the front shell so it is one mesh, and
the button holes get real caps.

Run inside Blender. Rebuilds from the STLs, so a lost .blend costs nothing.
"""
import bpy, math, os

SRC = "/var/home/hxshino/projects/Poket/blender/"
FACE_Z = 0.0106                      # front face of a unit, object-local, metres

# ------------------------------------------------------------------ geometry
base = SRC + "build_scene.py"
exec(compile(open(base).read(), base, 'exec'), {"__name__": "__main__", "MODE": "assembly"})

sc = bpy.context.scene
D  = bpy.data.objects
for n in ("Floor", "Key", "Fill", "Rim", "Camera", "CamTarget"):
    if n in D: bpy.data.objects.remove(D[n], do_unlink=True)
unit = D["Pivot"]; unit.name = "Poket_A"
for n in ("PCB", "FrontShell", "SliderCap", "BackShell", "Screen", "Poket_A"):
    D[n].animation_data_clear()
    D[n].location = (0, 0, 0); D[n].rotation_euler = (0, 0, 0)

front = D["FrontShell"]

# ---- "Poket" on the front face ----
# Cut a recess, then drop a light inlay into it and JOIN the inlay into the
# shell, so the wordmark is part of the same object but still legible through
# clear plastic (a bare engraving in transparent plastic reads as nothing).
FONT = "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf"
try:
    vfont = bpy.data.fonts.load(FONT)
except Exception:
    vfont = None

def wordmark(name, size, extrude, z):
    tc = bpy.data.curves.new(name, type='FONT')
    tc.body, tc.size, tc.extrude = "Poket", size, extrude
    if vfont: tc.font = vfont
    tc.align_x = tc.align_y = 'CENTER'
    o = bpy.data.objects.new(name, tc); sc.collection.objects.link(o)
    o.location = (-0.0180, 0.0223, z)
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.convert(target='MESH')
    return bpy.context.object

LOGO_SIZE, DEPTH = 0.0072, 0.0009
cut = wordmark("LogoCut", LOGO_SIZE, DEPTH, FACE_Z + 0.0002)
m = front.modifiers.new("Engrave", 'BOOLEAN')
m.operation, m.object, m.solver = 'DIFFERENCE', cut, 'EXACT'
bpy.ops.object.select_all(action='DESELECT')
front.select_set(True); bpy.context.view_layer.objects.active = front
bpy.ops.object.modifier_apply(modifier="Engrave")
bpy.data.objects.remove(cut, do_unlink=True)

inlay = wordmark("LogoInlay", LOGO_SIZE * 0.985, DEPTH * 0.42, FACE_Z - DEPTH * 0.38)
ink = bpy.data.materials.new("Logo_ink"); ink.use_nodes = True
ib = ink.node_tree.nodes["Principled BSDF"]
ib.inputs["Base Color"].default_value = (0.90, 0.92, 0.95, 1)
ib.inputs["Roughness"].default_value = 0.42
inlay.data.materials.append(ink)

bpy.ops.object.select_all(action='DESELECT')
inlay.select_set(True); front.select_set(True)
bpy.context.view_layer.objects.active = front
bpy.ops.object.join()                      # one object, two material slots
bpy.ops.object.select_all(action='DESELECT')
print("front shell + wordmark joined:", front.name, len(front.data.materials), "materials")

# ---- caps in the button holes and a knob on the encoder ----
def cap(name, x, y, r, z0, z1, bevel=0.0004):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=z1 - z0,
                                        location=(x, y, (z0 + z1) / 2), vertices=48)
    o = bpy.context.object; o.name = name
    b = o.modifiers.new("bev", 'BEVEL')
    b.width, b.segments, b.limit_method = bevel, 3, 'ANGLE'
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier="bev")
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(30))
    bpy.ops.object.select_all(action='DESELECT')
    o.parent = unit
    o.matrix_parent_inverse = unit.matrix_world.inverted()
    return o

btn_mat = bpy.data.materials.new("Button_mat"); btn_mat.use_nodes = True
bb = btn_mat.node_tree.nodes["Principled BSDF"]
bb.inputs["Base Color"].default_value = (0.030, 0.032, 0.038, 1)
bb.inputs["Roughness"].default_value = 0.34
knob_mat = bpy.data.materials.new("Knob_mat"); knob_mat.use_nodes = True
kb = knob_mat.node_tree.nodes["Principled BSDF"]
kb.inputs["Base Color"].default_value = (0.055, 0.058, 0.065, 1)
kb.inputs["Roughness"].default_value = 0.28
if "Metallic" in kb.inputs: kb.inputs["Metallic"].default_value = 0.35

CX, CY = -39.96, -26.973                    # case coords -> centred metres
for i, (bx, by) in enumerate(((24, 11), (33, 11), (42, 11))):
    o = cap("Btn%d" % (i + 1), (bx + CX) * 0.001, (by + CY) * 0.001,
            0.00255, FACE_Z - 0.0022, FACE_Z + 0.0007)
    o.data.materials.append(btn_mat)
kn = cap("Knob", (59.5 + CX) * 0.001, (18.3 + CY) * 0.001, 0.0035, FACE_Z - 0.0008, FACE_Z + 0.0044, 0.0006)
kn.data.materials.append(knob_mat)

# ------------------------------------------------------------------ material
mat = bpy.data.materials["Shell_PETG"]
sh = mat.node_tree.nodes["Principled BSDF"]
sh.inputs["Base Color"].default_value = (0.72, 0.74, 0.77, 1)
sh.inputs["Roughness"].default_value = 0.05
sh.inputs["IOR"].default_value = 1.52
sh.inputs["Transmission Weight"].default_value = 1.0
for k, v in (("Coat Weight", 0.30), ("Coat Roughness", 0.05)):
    if k in sh.inputs: sh.inputs[k].default_value = v
vol = mat.node_tree.nodes.new("ShaderNodeVolumeAbsorption"); vol.location = (10, -320)
vol.inputs["Color"].default_value = (0.52, 0.56, 0.62, 1)
vol.inputs["Density"].default_value = 150.0
out = [n for n in mat.node_tree.nodes if n.type == 'OUTPUT_MATERIAL'][0]
mat.node_tree.links.new(vol.outputs["Volume"], out.inputs["Volume"])
cp = bpy.data.materials["Cap_PETG"].node_tree.nodes["Principled BSDF"]
cp.inputs["Base Color"].default_value = (0.78, 0.32, 0.08, 1)
cp.inputs["Roughness"].default_value = 0.18
cp.inputs["Transmission Weight"].default_value = 0.5

# ------------------------------------------------------------------ two units
bpy.ops.object.select_all(action='DESELECT')
for o in list(sc.objects):
    if o.type in ('MESH', 'EMPTY'): o.select_set(True)
bpy.context.view_layer.objects.active = unit
bpy.ops.object.duplicate(linked=False)
dup = [o for o in bpy.context.selected_objects]
B = [o for o in dup if o.parent is None][0]; B.name = "Poket_B"
bpy.ops.object.select_all(action='DESELECT')
A = unit

# both hang in the air. The tilted one is the hero: steep, so the camera sees
# its face nearly square on, with the flat one tucked under it to the right.
FLOAT, TILT, GAP = 0.010, math.radians(62), 0.004
s, c = math.sin(TILT), math.cos(TILT)
low = min(y * s + z * c for y in (-0.030, 0.030) for z in (-0.0062, FACE_Z))
A.location = (0.030, -0.030, FLOAT)
A.rotation_euler = (0, 0, math.radians(8))
B.rotation_euler = (TILT, 0, math.radians(-18))
B.location = (-0.008, 0.004, FLOAT + FACE_Z - low + GAP)

# ------------------------------------------------------------------ stage
w = sc.world; w.use_nodes = True
wn = w.node_tree; wn.nodes.clear()
wout = wn.nodes.new("ShaderNodeOutputWorld"); wout.location = (700, 0)
bg   = wn.nodes.new("ShaderNodeBackground");  bg.location = (500, 0)
ramp = wn.nodes.new("ShaderNodeValToRGB");    ramp.location = (250, 0)
sep  = wn.nodes.new("ShaderNodeSeparateXYZ"); sep.location = (50, 0)
tcd  = wn.nodes.new("ShaderNodeTexCoord");    tcd.location = (-160, 0)
wn.links.new(tcd.outputs["Window"], sep.inputs["Vector"])
wn.links.new(sep.outputs["Y"], ramp.inputs["Fac"])
wn.links.new(ramp.outputs["Color"], bg.inputs["Color"])
wn.links.new(bg.outputs["Background"], wout.inputs["Surface"])
cr = ramp.color_ramp
cr.elements[0].position = 0.0; cr.elements[0].color = (0.030, 0.050, 0.095, 1)
cr.elements[1].position = 1.0; cr.elements[1].color = (0.003, 0.005, 0.012, 1)
cr.elements.new(0.45).color = (0.012, 0.020, 0.045, 1)

bpy.ops.mesh.primitive_plane_add(size=3.0, location=(0, 0, -0.004))
fl = bpy.context.object; fl.name = "Stage"
fm = bpy.data.materials.new("Stage_mat"); fm.use_nodes = True
fb = fm.node_tree.nodes["Principled BSDF"]
fb.inputs["Base Color"].default_value = (0.004, 0.006, 0.013, 1)
fb.inputs["Roughness"].default_value = 0.38
fl.data.materials.append(fm)

def area(name, loc, rot, size, energy, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, type='AREA'); d.size, d.energy, d.color = size, energy, color
    o = bpy.data.objects.new(name, d); sc.collection.objects.link(o)
    o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
# big soft source just off camera-left makes the left edge bloom the way the
# reference does; a hard rim from upper right picks out the chamfers
area("Glow", (-0.195, -0.150, 0.075), ( 80, 0, -55), 0.26, 8.0)
area("RimR", ( 0.250,  0.140, 0.175), ( 62, 0, 118), 0.20,  9.0, (0.84, 0.90, 1.0))
area("Top",  ( 0.020,  0.060, 0.330), (  8, 0,   0), 0.30,  1.4)
area("Fill", ( 0.060, -0.300, 0.020), ( 88, 0,  12), 0.30,  0.8, (0.80, 0.86, 1.0))

cd = bpy.data.cameras.new("HeroCam"); cd.lens = 85
cd.dof.use_dof = True; cd.dof.aperture_fstop = 4.0
cam = bpy.data.objects.new("HeroCam", cd); sc.collection.objects.link(cam)
cam.location = (0.100, -0.318, 0.082)     # close and near level with the hero unit
tgt = bpy.data.objects.new("HeroTgt", None); sc.collection.objects.link(tgt)
tgt.location = (-0.006, 0.000, 0.050)
tr = cam.constraints.new('TRACK_TO'); tr.target = tgt
tr.track_axis, tr.up_axis = 'TRACK_NEGATIVE_Z', 'UP_Y'
cd.dof.focus_object = tgt
sc.camera = cam

# ------------------------------------------------------------------ Cycles GPU
prefs = bpy.context.preferences.addons["cycles"].preferences
prefs.compute_device_type = 'OPTIX'
prefs.get_devices()
for d in prefs.devices: d.use = (d.type == 'OPTIX')
sc.render.engine = 'CYCLES'
sc.cycles.device = 'GPU'
sc.cycles.samples = 256
sc.cycles.use_denoising = True
sc.cycles.max_bounces = 24
sc.cycles.transmission_bounces = 16
sc.cycles.transparent_max_bounces = 24
sc.render.resolution_x, sc.render.resolution_y = 1280, 1060
sc.view_settings.view_transform = 'AgX'
try: sc.view_settings.look = 'AgX - Medium High Contrast'
except TypeError: pass
sc.render.image_settings.file_format = 'PNG'
sc.render.filepath = SRC + "hero"
bpy.ops.wm.save_as_mainfile(filepath=SRC + "poket-hero.blend")
print("hero scene ready | %s on %s | %d samples"
      % (sc.render.engine, sc.cycles.device, sc.cycles.samples))
