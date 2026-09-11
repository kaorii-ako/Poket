#!/usr/bin/env bash
# Stitch a Blender PNG sequence into MP4 + GIF. This Blender build has no
# ffmpeg writer compiled in, so encoding happens out here.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p video
enc () {  # enc <frame-dir> <prefix> <out-basename>
  ffmpeg -y -loglevel error -framerate 30 -i "$1/$2%04d.png" \
    -c:v libx264 -pix_fmt yuv420p -crf 18 -preset slow -movflags +faststart \
    "video/$3.mp4"
  ffmpeg -y -loglevel error -framerate 30 -i "$1/$2%04d.png" \
    -vf "fps=15,scale=560:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" \
    "video/$3.gif"
  echo "video/$3.mp4  $(du -h "video/$3.mp4" | cut -f1)"
}
mp4 () {  # mp4 <frame-dir> <prefix> <out-basename> - video only, no GIF
  ffmpeg -y -loglevel error -framerate 30 -i "$1/$2%04d.png" \
    -c:v libx264 -pix_fmt yuv420p -crf 17 -preset slow -movflags +faststart \
    "video/$3.mp4"
  echo "video/$3.mp4  $(du -h "video/$3.mp4" | cut -f1)"
}

[ -d blender/frames    ] && enc blender/frames    f_ poket-assembly
[ -d blender/promo     ] && enc blender/promo     p_ poket-promo
[ -d blender/ad/frames ] && mp4 blender/ad/frames a_ poket-ad
