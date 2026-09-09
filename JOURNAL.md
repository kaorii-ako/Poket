---
title: "Poket"
author: "hxshino"
description: "ESP32-S3 bluetooth MP3 player. microSD, PCM5102A DAC, headphone amp, LiPo, 3D printed case."
created_at: "2026-09-09"
---

# Sep 9: Scoping + part picks

Nailed down what this actually is: plays MP3s off a microSD, sends to BT
headphones (A2DP source) OR out a 3.5mm jack. OLED + encoder + 3 buttons.
LiPo with USB-C charging and a real fuel gauge.

Part hunt in the KiCad libs:
- ESP32-S3-WROOM-1-N16R8 (module, not bare chip — no RF matching to get wrong)
- PCM5102A I2S DAC. no MCLK needed, internal PLL. one less pin to burn
- PAM8908 headphone amp. charge pump = ground-centered output = no fat
  DC blocking caps in the audio path
- MCP73831 charger + DMG2301L load-share P-FET
- AP2112K-3.3 LDO
- Wanted MAX17048 for the fuel gauge but there's no symbol in KiCad and I
  wasn't confident enough in the pinout from memory. Went BQ27441-G1
  instead — symbol exists, guaranteed right. Costs me a 10mR sense
  resistor in the pack ground.

Pin map done on paper first. Had to dodge GPIO35/36/37 (octal PSRAM on
the R8 module) and 19/20 (native USB).

**Total time spent: 2 hours**

# Sep 9: Schematic — power + charge block

Project created, sheet bumped to A1 because this is not fitting on A4.

Power block in. USB-C 16P with 5.1k CCs, USBLC6 for ESD. Charger at 500mA
(2k on PROG). Load share is VBUS->Schottky->VSYS, and a P-FET from VBAT
that gets turned off by VBUS on its gate.

Got the gate divider wrong the first time — 100k/100k puts the gate at
2.5V while the source sits at 4.2V, so Vgs is -1.7V and the FET is still
half on. Fixed: 10k straight from VBUS to gate, 1M pulldown. Now it's a
clean off.

Power switch is a slide switch on the LDO EN pin, not in the battery
path. No amps through a cheap slide switch.

**Total time spent: 2 hours**

# Sep 9: Schematic done — audio, storage, UI, ERC clean

Rest of the schematic in one sitting.

Audio chain: PCM5102A -> 1uF coupling -> PAM8908 -> 3.5mm jack. Tied SCK
low so the DAC runs off its internal PLL, FMT/DEMP/FLT low for plain I2S.
Jack has a switched tip so I get plug-detect on a GPIO for free.

microSD wired for 4-bit SDIO (not SPI) with 10k pullups on CMD and all
four data lines. Card detect on the DM3AT's detect switch.

Pin map final. Dodged IO35/36/37 (PSRAM), IO19/20 (USB), and left the
strapping pins (IO0/3/45/46) alone except boot.

ERC caught two things:
1. BQ27441 BAT pin "not driven" — it sits behind a 1k filter resistor so
   nothing drives it as far as ERC cares. PWR_FLAG on that net.
2. I'd stuck a PWR_FLAG on VBAT, which is already driven by the charger's
   VBAT output pin. Two power outputs on one net = error. Moved it.

Also caught myself wiring the encoder wrong — put GND on B and the signal
on C. On the KiCad symbol C is the common. Swapped.

0 errors, 0 warnings.

![schematic](images/schematic.png)

**Total time spent: 4 hours**
