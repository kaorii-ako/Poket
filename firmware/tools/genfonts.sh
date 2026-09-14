#!/usr/bin/env bash
# Regenerate every font table. Run from firmware/tools.
#   ./genfonts.sh
# Writes main/ui/fonts_gen.c (firmware) and web/proto/fonts.js (browser
# preview) from the same source, so the preview cannot drift from the panel.
# Faces are named by fontconfig family - see the note at the top of mkfont.py.
set -euo pipefail
cd "$(dirname "$0")"

C=../main/ui/fonts_gen.c
J=../web/lib/fonts.js
: > "$C"; : > "$J"

gen() {   # gen <name> <spacing> <source-args...>
  local name=$1 sp=$2; shift 2
  python3 mkfont.py --name "$name" --spacing "$sp" --out "$C" --format c  "$@"
  python3 mkfont.py --name "$name" --spacing "$sp" --out "$J" --format js "$@" >/dev/null
}

# The two small faces are hand-drawn: an outline rasterised at 7-8px loses its
# stems, and a 1-bit panel has no antialiasing to hide that with.
gen font_sans_8  1 --builtin 5x8
gen font_mono_8  1 --builtin 5x8-mono

gen font_sans_11  1 --family "Liberation Sans" --size 11
gen font_sans_16  1 --family "Liberation Sans" --size 16
gen font_slab_22  1 --family "Liberation Serif" --weight bold --size 22
gen font_bold_10  1 --family "Liberation Sans" --weight bold --size 10
gen font_mono_12  0 --family "Liberation Mono" --weight bold --size 12
gen font_round_13 1 --family "Cantarell" --size 13
gen font_round_9  1 --family "Cantarell" --weight bold --size 10
