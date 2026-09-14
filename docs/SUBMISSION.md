# Submission checklist

## What's in the repo

| Requirement | Where | Status |
|---|---|---|
| Devlog / journal | [`JOURNAL.md`](../JOURNAL.md) | 11 entries, ~34 h, written as the work happened |
| Schematic (source) | `poket.kicad_sch` | KiCad 10, ERC **0 errors, 0 warnings** |
| Schematic (PDF / PNG) | [`docs/poket-schematic.pdf`](poket-schematic.pdf), `images/schematic.png` | regenerated from the Rev B board |
| PCB layout (source) | `poket.kicad_pcb` | DRC **0 errors, 0 schematic-parity issues**, every signal net routed |
| Gerbers + drill | `fab/poket-gerbers.zip` | 4-layer, JLCPCB-ready, Rev B |
| Pick-and-place | `fab/poket-pos.csv` | Rev B |
| BOM with costs | [`docs/BOM.md`](BOM.md), [`docs/BOM.csv`](BOM.csv) | costs are **estimates**; supplier P/Ns not filled in |
| CAD source | `cad/enclosure.py` | parametric, rebuilds from the script |
| Printable files | `enclosure/*.stl` | 3 parts, all single valid solids, 0.0000 mm³ interference vs the board |
| Renders / video | `images/`, `video/` | board renders regenerated for Rev B |
| License | [`LICENSE`](../LICENSE) | MIT (code) + CERN-OHL-S v2 (hardware) |
| Assembly instructions | [`docs/ASSEMBLY.md`](ASSEMBLY.md) | includes the Rev B parts |
| **Firmware** | `firmware/` | **written and building** — ESP-IDF v5.3, `poket.bin` 1.5 MB, 64 % of the app partition free |
| Flashing instructions | [`firmware/README.md`](../firmware/README.md), README | build, flash, first run, controls |
| Design reviewed by another person | — | **not yet done** — see the README's Design review section |
| Photos of real hardware | — | **never built** |

## The firmware

ESP-IDF v5.3, target `esp32`. `idf.py build` succeeds from a clean tree.

- **Audio** — vendored minimp3, one sink interface behind I2S (PCM5102A) and
  A2DP source. Bluetooth linking is implemented properly: inquiry, Class-of-
  Device filter for audio sinks, connect, then `CHECK_SRC_RDY` → `MEDIA_CTRL_START`.
  The headset is remembered in NVS and reconnected at boot.
- **Storage** — microSD over SPI, library scan, ID3 tags, playlists as `.m3u`
  files in `/Music/Playlists` so any other player can read them.
- **UI** — 1-bit graphics layer and six themes that differ in layout, type,
  iconography and motion, because a 1-bit panel has no colour to vary. Custom
  theme packs are interpreted as *data*, never executed.
- **Web app** — served gzipped from rodata, 30 kB, no external requests. Upload,
  library with search, playlists, transport, shuffle/repeat, Bluetooth pairing,
  and a live 128×64 screen preview that runs the firmware's own drawing code
  against the same glyph bytes.

## Honest gaps

**1. Nothing has been physically built.** Every image is a render. No board has
been fabricated, no part soldered, no measurement taken, and no firmware has
ever run on hardware. The design is ERC/DRC-clean, the enclosure is
boolean-verified against the real board outline, and the firmware compiles — but
"it passes DRC and builds" and "it works" are very different claims, and only
the first is being made here.

**2. Three ground-pour fragments are untied.** Two of them carry a bypass
capacitor's ground (C19, C33). The plane itself is intact and every signal net
is routed, but those two caps' ground return is not ideal. The fix is to move
the caps and re-route; noted rather than hidden.

**3. The BOM has no supplier part numbers.** Manufacturer part numbers are real
and chosen deliberately; LCSC/DigiKey numbers are blank rather than guessed.

**4. Prices are estimates**, not quotes. ~$46 is the right order of magnitude.

**5. Nobody else has reviewed it.** A second pair of eyes before fabrication is
the cheapest bug-finding there is, and this design has not had one. The README
lists what a reviewer should look at hardest.

**6. Rev A was wrong about Bluetooth.** The first board used an ESP32-S3, which
has Bluetooth LE only and physically cannot do A2DP. That is documented in
[`DESIGN.md`](DESIGN.md) and in the devlog rather than quietly corrected — it is
the most useful thing I learned building this.

## Is it ready?

As a **design and firmware submission**: yes, with one caveat — it has not been
reviewed by another person, which is worth doing before anyone spends money
having it made.

On the substance: Schematic, layout, fabrication
outputs, enclosure, and a complete firmware source tree that builds, all
consistent with each other and all regenerated from the current revision.

As a **working device**: no, and it does not claim to be. It has never been
built. Everything above describes what the design and the code do, not what a
physical unit has been observed to do.
