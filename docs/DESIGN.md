# Poket — design decisions and trade-offs

## What it is

An ESP32-S3 Bluetooth MP3 player. Local MP3s on a microSD card, played
out either over Bluetooth A2DP (as a *source*, to headphones/speakers) or
through a wired 3.5 mm jack. 0.96" OLED, rotary encoder with push, three
transport buttons, single-cell LiPo with USB-C charging.

---

## Electrical

### MCU — ESP32-S3-WROOM-1-N16R8, not a bare chip
The module costs a few dollars more than the bare die plus flash plus
PSRAM, and it saves the antenna matching network, the RF layout review,
and a pile of risk on a first spin. 16 MB flash gives room for the
firmware and a filesystem; 8 MB octal PSRAM gives the decode buffers you
want if you ever add gapless playback or a bigger UI.

Cost: the octal-PSRAM variant burns GPIO35/36/37 internally, so those are
off-limits. Native USB takes GPIO19/20. Everything else was assigned
around those.

### Audio chain — PCM5102A → PAM8908 → jack
The PCM5102A has an internal PLL, so it runs from BCK/LRCK alone and I
don't have to spend a GPIO and a clock domain on MCLK. Its output is
ground-centred thanks to an on-chip charge pump, which kills the usual
big DC-blocking electrolytics.

It can't drive 32 Ω headphones though (2.1 Vrms into 5 kΩ is a line
output), so the PAM8908 does the current. It's also a DirectDrive-style
charge-pump part — ground-referenced output, no output coupling caps, no
turn-on thump. Gain pins strapped for 0 dB; the amp is enabled from
GPIO21 so firmware can mute it before the DAC settles.

**Trade-off:** the PAM8908 is a QFN-16 with a thermal pad. Not
hand-solder friendly. If this were meant for hand assembly I'd have taken
the TPA6132A2 in the same pinout family, or dropped back to a jellybean
op-amp buffer and accepted the coupling caps.

### Fuel gauge — BQ27441-G1, not the MAX17048 I wanted
The MAX17048 is the nicer part for this job: ModelGauge estimates state of
charge from voltage alone, so there's no sense resistor and no
interruption of the pack ground. But there's no symbol for it in the
stock KiCad libraries and I wasn't confident enough in the TDFN pinout
from memory to draw one — a wrong pin here is a dead board.

The BQ27441-G1 has a verified KiCad symbol, so that's what went in. It's
a real coulomb counter, which is arguably *more* accurate, at the cost of
a 10 mΩ sense resistor in series with the pack negative and a slightly
more careful ground layout (SRP on the battery side, SRN on system
ground).

### Power path
```
USB-C ──┬── USBLC6 ESD ── D+/D- ── ESP32-S3
        ├── MCP73831 (500 mA) ── VBAT ── LiPo
        └── Schottky ──┐
                       ├── VSYS ── slide switch on LDO EN ── AP2112K-3.3 ── 3V3
   VBAT ── P-FET ──────┘                                                    │
          (off when VBUS present)                                    ferrite ┴ 3V3A
```

Two decisions worth calling out:

**Load-share gate divider.** First pass used 100 k / 100 k from VBUS to
the P-FET gate. That puts the gate at 2.5 V while the source sits at
4.2 V — Vgs = −1.7 V, so the FET is *partially on* and the USB supply
back-feeds the battery through it. Changed to 10 k from VBUS to the gate
and a 1 M pulldown: gate sits at ~5 V with USB in (hard off) and at 0 V
without it (hard on).

**LDO, not a buck-boost.** A TPS63020 would hold 3.3 V down to a 3.0 V
cell and be ~90 % efficient instead of ~80 %. An AP2112K is a $0.10
SOT-23-5 and a basic JLC part. For a first build I took the simple one and
accepted that the usable cell range stops around 3.4 V. The board will
run below that (the ESP32-S3 and both audio parts are happy at 3.0 V),
the rail just isn't regulated any more.

**Analog rail.** 3V3A is split off through a ferrite bead with its own
bulk + bypass, feeding the DAC and amp only. Cheap insurance against the
radio and the SD card putting hash into the audio.

### microSD on 4-bit SDIO, not SPI
Four data lines instead of one, roughly 4× the read bandwidth for the
same clock. MP3 doesn't need it, but it means the card is idle more of
the time, which matters when the radio is also asking for the bus. 10 k
pull-ups on CMD and all four data lines; card detect on the socket's
detect switch.

### Board stackup — 4 layer
Sig / GND / GND / Sig. Two solid ground planes is overkill for a
1 MHz-ish design, but it makes the I2S and SDIO returns trivial, keeps
the analog section quiet, and costs almost nothing at JLC in this size.

### The ESP32 antenna keepout
KiCad's `ESP32-S3-WROOM-1` footprint carries a 48 × 21 mm copper keepout
zone for the antenna, applied to every layer, plus a courtyard outline of
the same shape. Both are far more conservative than the module datasheet
needs and they make placement on an 80 × 54 board impossible.

I removed the vendor zone from the board instance, replaced the courtyard
with a plain rectangle around the module body, and shaped all four ground
pours to leave a notch (x 52–74, y 0–9.5 mm) open under and beside the
antenna instead. Same intent, sized to the part.

---

## Mechanical

### Layout logic
The front face is what you look at, so the screen, buttons and knob are
all top-side. Two things went to the *bottom* of the board on purpose:

- **microSD socket** — it's 15 mm deep. On the top side it would have
  eaten a fifth of the usable area; underneath, it tucks into the space
  above the battery and its slot lines up with the left wall.
- **BOOT and RESET** — these are setup buttons, not user controls.
  Pinholes in the back shell instead of two extra holes in the face.

### Case
86 × 60 × 16.8 mm, two shells split 1 mm above the PCB plane.

- **Back shell** is a tray: 1.5 mm floor, four 4.7 mm posts that the PCB
  sits on, and ribs that pen the battery into a 37 × 26 mm pocket so it
  can't slide onto the BOOT/RESET pinholes.
- **Front shell** is the cover, with the display window, three button
  holes, the encoder shaft hole, two LED pinholes, the power-slider slot,
  and five debossed vent slots top-right that are purely decoration.
- **Registration** is a 0.7 mm tongue on the front shell dropping into a
  0.25 mm-clearance rebate in the back shell wall.
- **Fastening** is four M2 × 12 screws from the back, through the posts
  and the PCB, into heat-set inserts in the front shell bosses. One screw
  does all three jobs.

### The slider cap
The power switch is an SMD slide switch whose actuator only stands about
2.5 mm off the PCB, but the front face is 7.5 mm above it — you'd never
reach it. So there's a third printed part: a 6 × 3 × 8.8 mm cap with a
pocket that grips the actuator and thumb grooves on top, poking through a
7.6 mm slot. 1.6 mm of travel, which matches the switch.

### Assumptions
- FDM print, PETG or PLA, 0.2 mm layers, 0.2 mm part clearance
- M2 heat-set inserts, 3.2 mm OD × 4 mm
- 402535-size LiPo (4 × 25 × 35 mm, ~400 mAh) in the battery bay
- 0.96" I2C SSD1306 module, 27.3 × 27.8 mm, on a 4-pin header

---

## Known compromises

1. **Screw near the antenna.** The top-right mounting hole is ~3 mm from
   the module's antenna edge. It's an unplated hole with no copper, but a
   steel M2 screw there isn't ideal. Moving it means an asymmetric screw
   pattern; I took the symmetry.
2. **No MCLK to the DAC.** Fine for the PCM5102A's internal PLL, but it
   rules out swapping in a DAC that needs a master clock without a board
   change.
3. **LDO efficiency.** See above — costs maybe 15 % of runtime versus a
   buck-boost.
4. **Screen is left of centre** on the front face, because the OLED
   module hangs off a header at a fixed spot. Reads as deliberate
   asymmetry (screen left, knob right) rather than a mistake, but it
   wasn't a free choice.
5. **QFN parts.** The amp and the fuel gauge both need reflow or a hot
   plate. This board wants to be assembled, not hand-built.
