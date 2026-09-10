# Building a Poket

## 1. Board

Order `fab/poket-gerbers.zip` as a 4-layer 80 × 54 mm board. If you're having
JLCPCB assemble it, `fab/poket-pos.csv` is the pick-and-place file and
`docs/BOM.csv` is the parts list — you'll need to fill in the supplier part
number column first.

Hand assembly order, easiest to hardest:

1. **U5 (PAM8908)** and **U6 (BQ27441)** first, while the board is empty and you
   can get a hot plate or hot air at them cleanly. U6 is 0.4 mm pitch — use
   plenty of flux, drag solder, then wick the bridges.
2. Remaining ICs, then passives, then connectors.
3. **J2 (microSD), SW5 and SW6 go on the BOTTOM side.** Easy to miss.
4. Encoder, buttons, jack, USB-C last.

Before powering anything: check for shorts between 3V3 and GND, and between
VBAT and GND.

## 2. First power-up

1. **Battery disconnected**, slide switch off. Plug in USB-C.
2. Slide switch on. Measure 3V3 at C5 — should be 3.3 V ±3 %.
3. Check 3V3A at C7 is the same (it comes through FB1).
4. Connect USB to a computer. The ESP32-S3 should enumerate as a USB device.
   If not, hold BOOT (back pinhole), tap RESET, release BOOT — that forces the
   ROM bootloader.
5. Only then connect the battery. D2 should light while charging.

## 3. Enclosure

Print all three parts from `enclosure/`:

| File | Orientation | Notes |
|---|---|---|
| `poket-front-shell.stl` | face down | no supports |
| `poket-back-shell.stl` | tray up | no supports |
| `poket-slider-cap.stl` | on its side | tiny, print with a brim |

Then:

1. Press four **M2 × 4 mm heat-set inserts** into the front shell bosses. Take
   your time — go in straight, let the iron do the work.
2. Push the slider cap onto the power switch actuator. It should be a friction
   fit; if it's loose, a dab of superglue.
3. Sit the battery in the ribbed pocket in the back shell, wire toward the
   bottom-left. Route the JST lead up the left side.
4. Drop the PCB onto the four posts, component side up.
5. Wire the OLED module to J4 (GND, 3V3, SCL, SDA — pin 1 is the square pad).
   Sit it in the front shell so the glass lines up with the window.
6. Close the shells and drive four **M2 × 12** screws in from the back.

## 4. Firmware

**Not written yet.** The board is designed around ESP-IDF with:

- `esp_a2dp_source` for Bluetooth audio out
- the SDMMC host peripheral in 4-bit mode for the card
- I2S in TX master mode to the DAC (no MCLK — the PCM5102A runs from BCK/LRCK)
- I2C for the OLED and the fuel gauge

See the pin map in `docs/DESIGN.md`.

## Gotchas that will bite you

- **The slide switch does not disconnect the battery.** It gates the LDO enable
  pin. The fuel gauge stays powered from VBAT permanently — that's deliberate,
  it needs to keep counting, but it means the cell drains slowly in storage.
- **GPIO35/36/37 are not free.** The N16R8 module uses them internally for the
  octal PSRAM.
- **There's no MCLK to the DAC.** Configure I2S accordingly or you'll get silence.
- **R24 is a 0.01 Ω sense resistor**, not a jumper. Fitting a 0 Ω here breaks
  the fuel gauge.
