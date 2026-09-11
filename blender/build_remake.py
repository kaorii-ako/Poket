"""Shot-for-shot remake of the reference phone film, with Poket.

Cut list lifted from the source with ffmpeg scene detection. The source runs
45.3 s; the last 10.9 s is a live-action talking head, which there is no way to
reproduce here, so this covers the 13 CG shots (0 -> 34.36 s) at the source's
25 fps and 1280x720, with the same durations and cut rhythm.

  #   in      out     source shot                 -> Poket equivalent
  1   0.00    3.24    wide, product tiny, drifts     same, edge-on
  2   3.24    6.64    macro along the top edge       chamfer + jack corner
  3   6.64   10.04    dark, red accent, lens macro   dark, orange slider macro
  4  10.04   13.32    front on, screen lit           same
  5  13.32   17.56    back detail -> fanned array    board through the case -> array
  6  17.56   19.84    thin side profile              USB-C edge
  7  19.84   22.56    macro of the back panel        PCB through the back
  8  22.56   23.36    top-edge module macro          3.5 mm jack macro
  9  23.36   24.28    extreme lens macro, bokeh      encoder knob macro
  10 24.28   24.64    fast cut, lenses + red square  buttons + orange slider
  11 24.64   26.36    back edge, coil                back edge, microSD slot
  12 26.36   31.44    three-quarter back, rotating   same
  13 31.44   34.36    wide, rotates to front         same
"""
import bpy, math, os
from mathutils import Matrix

SRC = "/var/home/hxshino/projects/Poket/blender/"
FPS = 25

# unit, engraved wordmark, button caps, translucent material, Cycles GPU
base = SRC + "build_hero.py"
exec(compile(open(base).read(), base, 'exec'), {"__name__": "__main__"})

sc = bpy.context.scene
D  = bpy.data.objects

# strip the hero staging - this film has its own
for n in [k for k in list(D.keys()) if k.endswith(".001") or k == "Poket_B"]:
    bpy.data.objects.remove(D[n], do_unlink=True)
for n in ("Stage", "Glow", "RimR", "Top", "Fill", "HeroCam", "HeroTgt"):
    if n in D: bpy.data.objects.remove(D[n], do_unlink=True)

unit = D["Poket_A"]
unit.animation_data_clear()
unit.location = (0, 0, 0); unit.rotation_euler = (0, 0, 0)

def fcurves_of(o):
    ad = o.animation_data
    if not ad or not ad.action: return []
    if hasattr(ad.action, "fcurves"): return list(ad.action.fcurves)
    return [fc for l in ad.action.layers for s in l.strips
            for fc in (s.channelbag(ad.action_slot).fcurves if s.channelbag(ad.action_slot) else [])]

def key(o, path, idx, frame, value, interp='SINE', easing='EASE_IN_OUT'):
    v = list(getattr(o, path)); v[idx] = value; setattr(o, path, v)
    o.keyframe_insert(path, index=idx, frame=frame)
    for fc in fcurves_of(o):
        if fc.data_path == path and fc.array_index == idx:
            for kp in fc.keyframe_points:
                if abs(kp.co[0] - frame) < 0.5:
                    kp.interpolation, kp.easing = interp, easing

# ------------------------------------------------------------------ cut list
CUTS = [0.00, 3.24, 6.64, 10.04, 13.32, 17.56, 19.84, 22.56,
        23.36, 24.28, 24.64, 26.36, 31.44, 34.36]
F = [int(round(t * FPS)) + 1 for t in CUTS]          # frame each shot starts on
END = F[-1] - 1
sc.frame_start, sc.frame_end = 1, END
sc.render.fps = FPS

# ------------------------------------------------------------------ world
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
cr.elements[0].position = 0.0; cr.elements[0].color = (0.52, 0.70, 0.90, 1)
cr.elements[1].position = 1.0; cr.elements[1].color = (0.006, 0.035, 0.175, 1)
cr.elements.new(0.45).color = (0.055, 0.190, 0.480, 1)

# shot 3 drops to near-black with a red accent, then comes back
st = bg.inputs["Strength"]
for f, v in ((F[1], 1.15), (F[2] - 2, 1.15), (F[2] + 4, 0.06),
             (F[3] - 4, 0.06), (F[3] + 4, 1.15)):
    st.default_value = v
    st.keyframe_insert("default_value", frame=f)
for fc in fcurves_of(wn):
    for kp in fc.keyframe_points:
        kp.interpolation, kp.easing = 'SINE', 'EASE_IN_OUT'

# ------------------------------------------------------------------ lights
def area(name, loc, rot, size, energy, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, type='AREA'); d.size, d.energy, d.color = size, energy, color
    o = bpy.data.objects.new(name, d); sc.collection.objects.link(o)
    o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
    return o
area("Key",  (-0.16, -0.26, 0.30), ( 30, 0, -26), 0.16, 3.4)
area("Top",  ( 0.02,  0.02, 0.34), (  0, 0,   0), 0.35, 1.8)
area("RimL", (-0.30,  0.20, 0.02), ( 74, 0, -128), 0.28, 7.5, (0.72, 0.83, 1.0))
area("RimR", ( 0.30,  0.22, 0.06), ( 72, 0,  126), 0.28, 9.0, (0.80, 0.88, 1.0))
# red kicker, only alive during shot 3
red = area("Red", (0.10, 0.055, 0.020), (78, 0, 150), 0.05, 0.0, (1.0, 0.09, 0.04))
rl = red.data
for f, v in ((F[2] - 1, 0.0), (F[2] + 3, 26.0), (F[3] - 3, 26.0), (F[3] + 1, 0.0)):
    rl.energy = v; rl.keyframe_insert("energy", frame=f)

# ------------------------------------------------------------------ the fan of units (shot 5)
fan = []
for i in range(6):
    bpy.ops.object.select_all(action='DESELECT')
    for o in list(sc.objects):
        if o == unit or o.parent == unit: o.select_set(True)
    bpy.context.view_layer.objects.active = unit
    bpy.ops.object.duplicate(linked=True)
    root = [o for o in bpy.context.selected_objects if o.parent is None][0]
    root.name = "Fan_%d" % i
    a = math.radians(-46 + i * 18)
    root.location = (0.115 * math.sin(a), 0.115 * math.cos(a) - 0.075, 0.0)
    root.rotation_euler = (math.radians(74), 0, a)
    fan.append(root)
bpy.ops.object.select_all(action='DESELECT')

def vis(objs, on_frames):
    """objs visible only inside [a, b]."""
    a, b = on_frames
    for r in objs:
        kids = [r] + [o for o in sc.objects if o.parent == r]
        for o in kids:
            for f, hide in ((1, True), (a - 1, True), (a, False), (b, False), (b + 1, True)):
                o.hide_render = o.hide_viewport = hide
                o.keyframe_insert("hide_render", frame=f)
                o.keyframe_insert("hide_viewport", frame=f)
            for fc in fcurves_of(o):
                for kp in fc.keyframe_points: kp.interpolation = 'CONSTANT'
vis(fan, (F[4] + 62, F[5] - 1))
vis([unit], (1, END))          # hero unit hidden while the fan is on screen
for o in [unit] + [x for x in sc.objects if x.parent == unit]:
    for f, hide in ((F[4] + 62, True), (F[5] - 1, True), (F[5], False)):
        o.hide_render = o.hide_viewport = hide
        o.keyframe_insert("hide_render", frame=f)
        o.keyframe_insert("hide_viewport", frame=f)
    for fc in fcurves_of(o):
        for kp in fc.keyframe_points: kp.interpolation = 'CONSTANT'

# ------------------------------------------------------------------ unit pose per shot
# A single continuous tumble left every macro camera aiming at empty space, so
# the pose is set explicitly at each cut and the macro cameras ride the unit.
POSE = {      # shot: (rot X, rot Y, rot Z start, rot Z end) in degrees
    0:  ( 16,   0,  -28,  -12),
    1:  ( 20,   0,    4,   14),
    2:  ( 18,   0,   -6,    2),
    3:  ( 80,   0,   -4,    4),     # front on, standing like a phone
    4:  (-96,   0,    6,   -6),     # back toward camera
    5:  (  0,  90,   82,   96),     # thin side profile, stood on its long edge
    6:  (-92,   0,   -4,    4),
    7:  ( 22,   0,    2,    8),
    8:  ( 18,   0,   -2,    4),
    9:  ( 16,   0,    0,    6),
    10: (-70,   0,   12,   20),
    11: (-58,   0,   26,   48),
    12: ( 18,   0,   58,  104),
}
for i, (rx, ry, z0, z1) in POSE.items():
    a, b = F[i], F[i + 1] - 1
    key(unit, "rotation_euler", 0, a, math.radians(rx), interp='CONSTANT')
    key(unit, "rotation_euler", 1, a, math.radians(ry), interp='CONSTANT')
    key(unit, "rotation_euler", 2, a, math.radians(z0), interp='LINEAR')
    key(unit, "rotation_euler", 2, b, math.radians(z1), interp='LINEAR')

# ------------------------------------------------------------------ cameras
def cam(name, lens, fstop, loc, look, parent=None):
    cd = bpy.data.cameras.new(name); cd.lens = lens
    cd.dof.use_dof = True; cd.dof.aperture_fstop = fstop
    c = bpy.data.objects.new(name, cd); sc.collection.objects.link(c)
    t = bpy.data.objects.new(name + "_T", None); sc.collection.objects.link(t)
    if parent:                       # ride the unit so macro framing survives rotation
        for o in (c, t):
            o.parent = parent
            o.matrix_parent_inverse = Matrix.Identity(4)
    c.location = loc; t.location = look
    tr = c.constraints.new('TRACK_TO'); tr.target = t
    tr.track_axis, tr.up_axis = 'TRACK_NEGATIVE_Z', 'UP_Y'
    cd.dof.focus_object = t
    return c

# features in unit-local metres: face at z=+0.0106, back at z=-0.0062
JACK   = (-0.0280,  0.0218,  0.0040)
GRILLE = ( 0.0200,  0.0230,  0.0110)
KNOB   = ( 0.0195, -0.0087,  0.0128)
BTNS   = (-0.0070, -0.0160,  0.0110)
SDSLOT = (-0.0430, -0.0030, -0.0020)
BACK   = ( 0.0000,  0.0000, -0.0062)

SHOTS = [
    ("S01_wide",  60, 6.0, ( 0.055,-0.700, 0.075), ( 0.020,-0.520, 0.045), (0,0,0),  None),
    ("S02_edge",  85, 8.0, ( 0.075,-0.085, 0.075), ( 0.045,-0.075, 0.060), GRILLE,  "u"),
    ("S03_dark",  85, 7.0, ( 0.045,-0.105, 0.060), ( 0.010,-0.095, 0.050), BTNS,    "u"),
    ("S04_front", 75, 6.0, ( 0.000,-0.330, 0.030), ( 0.000,-0.300, 0.020), (0,0,0),  None),
    ("S05_back",  95, 4.5, ( 0.010,-0.150,-0.020), (-0.070,-0.330,-0.010), (0,0,0),  None),
    ("S06_side",  95, 6.0, ( 0.000,-0.250, 0.010), ( 0.000,-0.215, 0.004), (0,0,0),  None),
    ("S07_pcb",   85, 8.0, ( 0.040,-0.090,-0.085), ( 0.010,-0.080,-0.070), BACK,    "u"),
    ("S08_jack",  85, 8.0, (-0.070,-0.085, 0.055), (-0.050,-0.075, 0.045), JACK,    "u"),
    ("S09_knob",  85,11.0, ( 0.078,-0.098, 0.088), ( 0.060,-0.084, 0.074), KNOB,    "u"),
    ("S10_btns",  85,10.0, ( 0.016,-0.118, 0.078), (-0.004,-0.102, 0.066), BTNS,    "u"),
    ("S11_sd",    85, 8.0, (-0.110,-0.070,-0.010), (-0.095,-0.058,-0.004), SDSLOT,  "u"),
    ("S12_34",    85, 5.0, ( 0.145,-0.245,-0.070), ( 0.085,-0.265,-0.030), (0,0,0),  None),
    ("S13_out",   70, 6.0, ( 0.045,-0.420, 0.030), ( 0.010,-0.500, 0.055), (0,0,0),  None),
]

for mk in list(sc.timeline_markers): sc.timeline_markers.remove(mk)
for i, (name, lens, fstop, p0, p1, look, ride) in enumerate(SHOTS):
    a, b = F[i], F[i + 1] - 1
    c = cam(name, lens, fstop, p0, look, unit if ride else None)
    for idx in range(3):
        key(c, "location", idx, a, p0[idx])
        key(c, "location", idx, b, p1[idx])
    sc.timeline_markers.new(name, frame=a).camera = c
sc.camera = D["S01_wide"]

# the red kicker for shot 3 rides the unit too, raking across the slider
red.parent = unit
red.matrix_parent_inverse = Matrix.Identity(4)
red.location = (0.038, -0.040, 0.026)
red.rotation_euler = [math.radians(a) for a in (55, 0, 150)]

# ------------------------------------------------------------------ output
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.cycles.samples = 64
sc.cycles.use_denoising = True
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGB'
os.makedirs(SRC + "remake", exist_ok=True)
sc.render.filepath = SRC + "remake/r_"
bpy.ops.wm.save_as_mainfile(filepath=SRC + "poket-remake.blend")
print("remake ready: %d shots, %d frames, %.2f s @ %d fps" % (len(SHOTS), END, END / FPS, FPS))
print("cuts at frames:", F)
