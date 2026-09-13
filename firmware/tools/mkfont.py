#!/usr/bin/env python3
"""Rasterise a TTF into a 1-bit C font table for ui/gfx.

There is no PIL or freetype binding on this machine, so the pipeline goes
  SVG (one glyph, chosen face+size)  ->  rsvg-convert  ->  PNG
  PNG  ->  ffmpeg -pix_fmt gray      ->  raw bytes
and thresholds from there. Ugly, but it uses tools that are already here.

Each glyph is rendered at SS times the target size and box-filtered down, so
the threshold sees real coverage per pixel. Thresholding the 1:1 render
instead loses any stem that lands on a half-covered column - at 9px that ate
the top arm off "E" - because the anti-aliased grey never clears the cut.
Below ~10px even this is not enough and the two small faces come from the
hand-drawn table in font5x8.py.

The face is named by fontconfig family, not by file: librsvg silently ignores
an @font-face src:url(), so pointing at a .ttf got the default sans every
time and all nine "different" faces came out identical.

  python3 mkfont.py --family "Liberation Sans" --size 14 \
                    --name font_sans_14 --out ../main/ui/fonts_gen.c
"""
import argparse, os, re, subprocess, sys, tempfile

import font5x8

CHARS = [chr(c) for c in range(0x20, 0x7F)]
PAD = 8
SS = 4            # supersample factor
COVERAGE = 0.42   # a cell is inked when this much of it is covered


def render(family, weight, px, ch, tmp):
    esc = {'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&apos;'}.get(ch, ch)
    w, h = px * 3 + PAD * 2, px * 3 + PAD * 2          # target, 1px per cell
    bw, bh = w * SS, h * SS                            # supersampled buffer
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{bw}" height="{bh}">'
           f'<rect width="100%" height="100%" fill="black"/>'
           f'<text x="{PAD * SS}" y="{(PAD + px) * SS}" font-size="{px * SS}px" fill="white" '
           f'font-family="{family}" font-weight="{weight}">{esc}</text>'
           f'</svg>')
    s = os.path.join(tmp, "g.svg"); p = os.path.join(tmp, "g.png"); r = os.path.join(tmp, "g.raw")
    open(s, "w").write(svg)
    subprocess.run(["rsvg-convert", "-w", str(bw), "-h", str(bh), s, "-o", p],
                   check=True, capture_output=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", p,
                    "-f", "rawvideo", "-pix_fmt", "gray", r],
                   check=True, capture_output=True)
    data = open(r, "rb").read()
    cut = COVERAGE * SS * SS * 255
    grid = []
    for y in range(h):
        row = []
        for x in range(w):
            acc = 0
            for j in range(SS):
                base = (y * SS + j) * bw + x * SS
                acc += sum(data[base:base + SS])
            row.append(1 if acc >= cut else 0)
        grid.append(row)
    return grid, w, h


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--family", help="fontconfig family, e.g. \"Liberation Sans\"")
    ap.add_argument("--weight", default="normal", choices=["normal", "bold"])
    ap.add_argument("--size", type=int)
    ap.add_argument("--builtin", choices=["5x8", "5x8-mono"],
                    help="use the hand-drawn table in font5x8.py instead of a TTF; "
                         "outline rasterisation falls apart below ~10px")
    ap.add_argument("--name", required=True)
    ap.add_argument("--spacing", type=int, default=1)
    ap.add_argument("--out", required=True)
    ap.add_argument("--format", choices=["c", "js"], default="c")
    a = ap.parse_args()
    if not a.builtin and not (a.family and a.size):
        ap.error("give either --builtin, or --family with --size")

    if a.builtin:
        prop = a.builtin == "5x8"
        height = font5x8.CELL_H
        data, offsets, widths = [], [], []
        for ch in CHARS:
            gw, rows = font5x8.glyph(ch, prop)
            stride = (gw + 7) // 8
            offsets.append(len(data)); widths.append(gw)
            for row in rows:
                packed = [0] * stride
                for i, v in enumerate(row):
                    if v:
                        packed[i >> 3] |= 0x80 >> (i & 7)
                data.extend(packed)
        emit(a, height, data, offsets, widths, f"hand-drawn {a.builtin} table")
        return

    glyphs, tmp = [], tempfile.mkdtemp()
    # one pass to find the common baseline box so every glyph shares rows
    boxes = []
    for ch in CHARS:
        g, w, h = render(a.family, a.weight, a.size, ch, tmp)
        rows = [y for y in range(h) if any(g[y])]
        cols = [x for x in range(w) if any(g[y][x] for y in range(h))]
        boxes.append((g, w, h, rows, cols))
    tops = [b[3][0] for b in boxes if b[3]]
    bots = [b[3][-1] for b in boxes if b[3]]
    top, bot = min(tops), max(bots)
    height = bot - top + 1

    data, offsets, widths = [], [], []
    for (g, w, h, rows, cols) in boxes:
        if not cols:                      # space
            gw = max(2, a.size // 3)
            offsets.append(len(data)); widths.append(gw)
            data.extend([0] * (((gw + 7) // 8) * height))
            continue
        x0, x1 = cols[0], cols[-1]
        gw = x1 - x0 + 1
        stride = (gw + 7) // 8
        offsets.append(len(data)); widths.append(gw)
        for y in range(top, bot + 1):
            row = [0] * stride
            for i in range(gw):
                if g[y][x0 + i]:
                    row[i >> 3] |= 0x80 >> (i & 7)
            data.extend(row)

    emit(a, height, data, offsets, widths,
         f"{a.family} {a.weight} @ {a.size}px")


def emit(a, height, data, offsets, widths, note):
    n = a.name
    if a.format == "js":
        # same glyph bytes the firmware uses, so the browser preview is not an
        # approximation of the panel - it is the panel's own rasterisation
        rows = []
        for i, ch in enumerate(CHARS):
            gw = widths[i]; stride = (gw + 7) // 8
            g = data[offsets[i]: offsets[i] + stride * height]
            rows.append("%d:[%d,%s]" % (0x20 + i, gw, "[" + ",".join(str(b) for b in g) + "]"))
        js = "export const %s = {h:%d,sp:%d,g:{%s}};\n" % (n, height, a.spacing, ",".join(rows))
        mode = "a" if os.path.exists(a.out) and os.path.getsize(a.out) > 0 else "w"
        with open(a.out, mode) as f:
            f.write(js)
        print("%-18s h=%2d  %5d bytes  [js]  %s" % (a.name, height, len(data), note))
        return
    out = []
    out.append(f"// generated by tools/mkfont.py from {note}")
    out.append(f"// {len(CHARS)} glyphs, {height}px tall, {len(data)} bytes")
    out.append('#include "ui/gfx.h"\n')
    out.append(f"static const uint8_t {n}_data[] = {{")
    for i in range(0, len(data), 16):
        out.append("    " + ", ".join("0x%02X" % b for b in data[i:i + 16]) + ",")
    out.append("};")
    out.append(f"static const uint16_t {n}_off[] = {{")
    for i in range(0, len(offsets), 12):
        out.append("    " + ", ".join(str(v) for v in offsets[i:i + 12]) + ",")
    out.append("};")
    out.append(f"static const uint8_t {n}_w[] = {{")
    for i in range(0, len(widths), 20):
        out.append("    " + ", ".join(str(v) for v in widths[i:i + 20]) + ",")
    out.append("};")
    out.append(f"const font_t {n} = {{ 0x20, 0x7E, {height}, {a.spacing}, "
               f"{n}_data, {n}_off, {n}_w }};\n")

    mode = "a" if os.path.exists(a.out) and os.path.getsize(a.out) > 0 else "w"
    with open(a.out, mode) as f:
        f.write("\n".join(out) + "\n")
    print("%-18s h=%2d  %5d bytes  %s" % (a.name, height, len(data), note))


if __name__ == "__main__":
    main()
