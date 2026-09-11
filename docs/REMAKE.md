# Shot-for-shot remake of the reference film

`video/poket-remake.mp4` is a beat-for-beat rebuild of the reference phone
launch film, with Poket in place of the phone.

## How the cut list was derived

Not by eye — ffmpeg scene detection on the source:

```
ffmpeg -i reference.mp4 -filter:v "select='gt(scene,0.30)',showinfo" -f null -
```

That gives 16 cuts. The last four shots (34.36 s onward) are a live-action
talking head at a desk. There is no way to reproduce those here, so the remake
covers the 13 CG shots, 0 → 34.36 s, at the source's own 25 fps and 1280 × 720,
with the same durations and the same cut rhythm.

| # | in | out | source shot | Poket equivalent |
|---|---|---|---|---|
| 1 | 0.00 | 3.24 | wide, product tiny, drifting | same, edge-on |
| 2 | 3.24 | 6.64 | macro along the top edge | chamfer + vent grille |
| 3 | 6.64 | 10.04 | dark, red accent, lens macro | dark, red kicker across the slider |
| 4 | 10.04 | 13.32 | front on, screen lit | same, stood upright |
| 5 | 13.32 | 17.56 | back detail → fanned array | board through the case → array of six |
| 6 | 17.56 | 19.84 | thin side profile | stood on its long edge |
| 7 | 19.84 | 22.56 | macro of the back panel | PCB through the back shell |
| 8 | 22.56 | 23.36 | top-edge module macro | 3.5 mm jack corner |
| 9 | 23.36 | 24.28 | extreme lens macro | encoder knob |
| 10 | 24.28 | 24.64 | fast cut, lenses + red square | buttons + orange slider |
| 11 | 24.64 | 26.36 | back edge, charging coil | back edge, microSD slot |
| 12 | 26.36 | 31.44 | three-quarter back, rotating | same |
| 13 | 31.44 | 34.36 | wide, rotates to front | same |

## Two things that needed solving

**The macro shots kept missing.** With one continuous tumble under the whole
film, a camera aimed at a fixed world point framed empty space by the time the
shot arrived. Fixed by parenting the macro cameras *to the unit* so they ride
it, and setting the unit's pose explicitly at every cut instead of letting one
rotation run through.

**Translucent shells eat macro depth of field.** At 100 mm and f/2.8 from 5 cm,
the out-of-focus near surface of the shell filled the frame and the subject
behind it never read. The macros now sit 8–12 cm out at f/8–f/11.

## Rebuilding

`blender/build_remake.py` constructs the whole thing from the STLs — it chains
through `build_hero.py` for the unit, engraving and translucent material.
Render the sequence, then:

```
ffmpeg -framerate 25 -i blender/remake/r_%04d.png -c:v libx264 \
       -pix_fmt yuv420p -crf 18 -preset slow video/poket-remake.mp4
```
