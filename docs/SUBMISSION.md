# Submission checklist

## What's in the repo

| Requirement | Where | Status |
|---|---|---|
| Devlog / journal | [`JOURNAL.md`](../JOURNAL.md) | ✅ 9 entries, ~27 h, written as the work happened |
| Schematic (source) | `poket.kicad_sch` | ✅ KiCad 10, ERC **0 errors 0 warnings** |
| Schematic (PDF) | [`docs/poket-schematic.pdf`](poket-schematic.pdf) | ✅ |
| PCB layout (source) | `poket.kicad_pcb` | ✅ DRC **0 errors, 0 unconnected, 0 parity** |
| Gerbers + drill | `fab/poket-gerbers.zip` | ✅ 4-layer, JLCPCB-ready |
| Pick-and-place | `fab/poket-pos.csv` | ✅ |
| BOM with costs | [`docs/BOM.md`](BOM.md), [`docs/BOM.csv`](BOM.csv) | ⚠️ costs are **estimates**; supplier P/Ns not filled in |
| CAD source | `cad/enclosure.py`, `cad/poket-enclosure.FCStd` | ✅ parametric, rebuilds from the script |
| Printable files | `enclosure/*.stl` | ✅ 3 parts, all single valid solids |
| Renders / video | `images/`, `video/` | ✅ |
| License | [`LICENSE`](../LICENSE) | ✅ MIT (code) + CERN-OHL-S v2 (hardware) |
| Assembly instructions | [`docs/ASSEMBLY.md`](ASSEMBLY.md) | ✅ |
| Firmware | — | ❌ **not written** |
| Photos of real hardware | — | ❌ **never built** |

## Honest gaps

**1. There is no firmware.** The board is designed to run ESP-IDF with
`esp_a2dp_source`, SDMMC 4-bit, I2S TX and I2C, and the pin map is chosen for
that, but not a line of it exists. Nothing here has been proven to play audio.

**2. Nothing has been physically built.** Every image is a render. No board has
been fabricated, no part soldered, no measurement taken. The design is
DRC/ERC-clean and the enclosure is boolean-verified against the real board
outline, but "it passes DRC" and "it works" are very different claims.

**3. The BOM has no supplier part numbers.** Manufacturer part numbers are real
and chosen deliberately; LCSC/DigiKey numbers are deliberately blank rather than
guessed. Fill them in before ordering.

**4. Prices are estimates from memory**, not quotes. The ~$46 total is the right
order of magnitude, not a number to budget against precisely.

## Is it ready?

**For a "fund my PCB" style submission — yes.** The deliverable those ask for is
a complete, manufacturable design with a real BOM and an honest build log, and
that's all here and verifiable: the gerbers will fab, the parts exist, the case
fits the board.

**For a "here is my finished working project" submission — no.** No firmware and
no physical build means the core claim is untested.

### Shortest path to closing the gap

1. Fill in supplier part numbers, order boards + parts (~2–3 weeks lead time).
2. Write firmware while they ship — SD read → I2S → DAC first, Bluetooth after.
3. Build one, photograph it, log what broke.
