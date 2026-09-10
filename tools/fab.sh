#!/usr/bin/env bash
# Generate fabrication outputs for JLCPCB (4 layer, 1.6 mm)
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p fab docs
kicad-cli pcb export gerbers --no-protel-ext -o fab/ poket.kicad_pcb
kicad-cli pcb export drill --format excellon --generate-map --map-format gerberx2 -o fab/ poket.kicad_pcb
kicad-cli pcb export pos --format csv --units mm --side both --use-drill-file-origin -o fab/poket-pos.csv poket.kicad_pcb
kicad-cli sch export bom --fields 'Reference,Value,Footprint,${QUANTITY}' --group-by 'Value,Footprint' \
  -o docs/poket-bom.csv poket.kicad_sch
kicad-cli sch export pdf -o docs/poket-schematic.pdf poket.kicad_sch
( cd fab && rm -f poket-gerbers.zip && zip -q poket-gerbers.zip *.gbr *.drl *.gbrjob 2>/dev/null || true )
echo "fab/ and docs/ updated"
