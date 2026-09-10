# Poket — bill of materials

98 parts across 41 line items. Machine-readable version: [`BOM.csv`](BOM.csv).

> **Prices are ballpark estimates, not quotes.** They're single-unit hobby
> pricing from memory, good enough to size the project, wrong enough that you
> should re-check every line before ordering. The **supplier part number column
> is deliberately empty** — I'm not going to invent LCSC `C` numbers, because a
> wrong one means you get the wrong part in the post. Search the MPN on
> LCSC/DigiKey/Mouser, confirm the package matches the Package column, and fill
> them in as you go.

## Cost summary

| | Est. |
|---|---|
| Electronics + modules | **$24.13** |
| PCB, screws, inserts, filament | **$21.60** |
| **Total for one unit** | **≈ $46** |

The PCB dominates the second line — JLCPCB's 4-layer minimum is 5 boards, so
per-board cost drops hard if you build more than one. Boards 2–5 cost roughly
$25 each in parts alone.

## Semiconductors

| Ref | Qty | Part | Package | What it does |
|---|---|---|---|---|
| U1 | 1 | ESP32-S3-WROOM-1-N16R8 | SMD module | MCU + Bluetooth radio. 16 MB flash, 8 MB octal PSRAM |
| U4 | 1 | PCM5102APWR | TSSOP-20 | I2S stereo DAC. Internal PLL, so no MCLK line needed |
| U5 | 1 | PAM8908JER | QFN-16 3×3 EP | Headphone amp. Charge pump gives ground-centred output — no DC blocking caps |
| U6 | 1 | BQ27441DRZR-G1A | SON-12 2×4 mm | Coulomb-counting fuel gauge, I2C |
| U2 | 1 | MCP73831T-2ACI/OT | SOT-23-5 | LiPo charger, 500 mA set by R3 |
| U3 | 1 | AP2112K-3.3TRG1 | SOT-23-5 | 3.3 V 600 mA LDO with enable |
| U7 | 1 | USBLC6-2SC6 | SOT-23-6 | USB ESD protection |
| Q1 | 1 | DMG2301L | SOT-23 | P-FET load-share switch |
| D3 | 1 | B5819WS | SOD-123 | Schottky, USB→VSYS |
| D1 | 1 | WS2812B-2020 | PLCC-4 2×2 mm | Addressable status LED |
| D2 | 1 | KT-0603G or equiv. | 0603 | Charge indicator |

**Two parts need reflow or a hot plate** — U5 (QFN with a thermal pad) and U6
(SON-12 at 0.4 mm pitch). This board wants to be assembled, not hand-built.

## Connectors, switches, display

| Ref | Qty | Part | Notes |
|---|---|---|---|
| J1 | 1 | HC-TYPE-C-16P-01A | USB-C receptacle, USB 2.0 |
| J2 | 1 | DM3AT-SF-PEJM5 | microSD, push-push, with card detect. **Mounted on the underside** |
| J3 | 1 | SJ1-3535NG | 3.5 mm jack, switched tip for plug detect |
| J5 | 1 | S2B-PH-SM4-TB | JST-PH battery connector |
| J4, J6 | 2 | 1×4 header 2.54 mm | OLED, UART debug |
| SW1 | 1 | EC11E, 20 mm shaft | Rotary encoder with push |
| SW2–4 | 3 | 6×6×5 mm tact | Prev / play / next |
| SW5–6 | 2 | B3U-1000P | BOOT / RESET, **underside**, reached through back-shell pinholes |
| SW7 | 1 | PCM12SMTR | Power slide switch — drives the LDO enable, not the battery current |
| DS1 | 1 | SSD1306 0.96" I2C, 128×64 | 27.3 × 27.8 mm module, 4-pin |
| BT1 | 1 | LiPo 402535, ~400 mAh | 4 × 25 × 35 mm. **Must have protection circuitry** |

## Passives

All 0603 unless noted. 1 % resistors, X7R/X5R ceramics.

| Value | Qty | Refs |
|---|---|---|
| 10 µF 0805 16 V | 9 | C1, C3–C5, C7, C9, C13, C16, C25 |
| 1 µF | 8 | C11, C18–C24 |
| 100 nF | 11 | C2, C6, C8, C10, C12, C14, C15, C26–C29 |
| 2.2 µF | 1 | C17 (DAC charge-pump flying cap) |
| 10 nF | 2 | C30, C31 (encoder RC) |
| 10 k | 18 | R5, R7, R8, R11–R19, R25–R30 |
| 100 k | 3 | R20–R22 |
| 5.1 k | 2 | R1, R2 (USB-C CC pulldowns — **both required**) |
| 4.7 k | 2 | R9, R10 (I2C) |
| 2 k | 1 | R3 (sets 500 mA charge current) |
| 1 k | 2 | R4, R23 |
| 1 M | 1 | R6 |
| 0.01 Ω 1206 1 W | 1 | R24 (fuel-gauge sense — **not a generic 0 Ω**) |
| 600 Ω @ 100 MHz bead | 1 | FB1 (isolates the 3V3A analog rail) |

## Fabrication and hardware

| Item | Qty | Spec |
|---|---|---|
| PCB | 5 (min order) | 80 × 54 mm, **4 layer**, 1.6 mm, HASL or ENIG |
| Heat-set inserts | 4 | M2 × 4 mm, 3.2 mm OD |
| Screws | 4 | M2 × 12 mm pan head |
| Filament | ~25 g | PETG or PLA |

### PCB order settings (JLCPCB)

| | |
|---|---|
| Layers | 4 |
| Dimensions | 80 × 54 mm |
| Thickness | 1.6 mm |
| Min track / clearance used | 0.20 mm / 0.15 mm |
| Min drill used | 0.20 mm |
| Impedance control | not required |

Upload `fab/poket-gerbers.zip`. The design uses nothing outside JLCPCB's
standard 4-layer capability (they do 0.127 mm track/space and 0.15 mm drill).

### Print settings

PETG or PLA, 0.2 mm layers, 4 perimeters, 20 % infill, **no supports needed**.
Front shell prints face-down, back shell tray-up, slider cap on its side.
