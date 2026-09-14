# Poket firmware

ESP-IDF v5.3, target `esp32` (ESP32-WROOM-32E-N16R2).

```
main/
  main.c            bring-up order: NVS, panel, card, audio
  board.h           the pin map, extracted from the schematic netlist
  app/              the one shared state snapshot + the UI task
  drivers/          I2C, SSD1306, encoder + buttons, BQ27441 fuel gauge
  storage/          microSD over SPI, library scan, ID3, .m3u playlists
  audio/            minimp3 -> one sink interface -> I2S or A2DP
  ui/               1-bit graphics, six themes, font tables
  net/              Wi-Fi AP, HTTP server, the gzipped web app
web/                the web app source; build.py bundles it into net/www.h
tools/              font generation
```

## Build

Install ESP-IDF v5.3, then:

```bash
. $HOME/esp/esp-idf/export.sh      # wherever you installed it
cd firmware
idf.py set-target esp32            # only needed once
idf.py build
```

`sdkconfig.defaults` already carries everything that matters — 16 MB flash,
PSRAM, Classic Bluetooth with A2DP, the custom partition table — so a clean
tree builds without touching menuconfig.

Expect roughly:

```
poket.bin binary size 0x1741d0 bytes. Smallest app partition is 0x400000 bytes.
```

## Flash

The board has no native USB — a CP2102N bridge sits on UART0, with the usual
cross-coupled auto-reset pair — so flashing works without touching any buttons.

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

On Linux the port is usually `/dev/ttyUSB0`, on macOS
`/dev/cu.SLAB_USBtoUART`, on Windows a `COM` port. `idf.py -p PORT monitor`
alone opens the serial log; Ctrl-] exits.

If auto-reset ever fails, hold **BOOT**, tap **RESET**, release **BOOT**, and
flash again. Both buttons are on the back of the board.

Erasing everything, including the saved Wi-Fi password, theme choice and
remembered headset:

```bash
idf.py -p /dev/ttyUSB0 erase-flash
```

### Flashing prebuilt binaries

Without ESP-IDF installed, `esptool` is enough:

```bash
esptool.py --chip esp32 -p /dev/ttyUSB0 -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/poket.bin
```

## First run

1. Put MP3s in `/Music` on a FAT32 microSD card and insert it.
2. Power on. The splash shows while the library is scanned.
3. **Long-press the encoder** to start transfer mode. The screen shows the
   Wi-Fi name (`Poket-XXXX`) and its password — generated on first boot and
   kept in NVS, so it does not change.
4. Join that network and open **http://192.168.4.1**.

From the web app: drop MP3s in, search the library, make playlists, drive
transport, pick one of the six screen themes with a live preview, and link a
pair of Bluetooth headphones.

### Linking headphones

Poket is the A2DP **source**, so it goes looking rather than waiting to be
found. Put the headphones in pairing mode, hit **Scan for headphones**, and pick
them from the list. The headset is saved and reconnected automatically on later
boots. **Long-press Play** toggles between the jack and Bluetooth.

Wi-Fi and Bluetooth share one radio, so audio can stutter while the web app is
open. In normal use it is one or the other.

## Controls

| Input | Action |
|---|---|
| Encoder turn | volume, or move the selection in the browser |
| Encoder press | browser / now playing |
| Encoder long-press | start or stop transfer mode |
| Play | play / pause |
| Play long-press | switch between the jack and Bluetooth |
| Prev / Next | previous (restarts if >3 s in) / next |

## Regenerating the generated files

Both are checked in, so neither is needed for an ordinary build.

```bash
cd tools && ./genfonts.sh     # main/ui/fonts_gen.c and web/lib/fonts.js
cd web    && ./build.py       # bundles the web app into main/net/www.h
```

The web app is inlined and gzipped into rodata because the access point has no
route to the internet: a single external `<script>` would leave the page blank.
`build.py` fails the build if the bundle exceeds 60 kB gzipped, or if two
modules declare the same top-level name.

```bash
node web/lib/clock.test.mjs   # playback-position arithmetic
```

## Known gaps

Nothing here has run on real hardware — no board has been fabricated. It builds
and the logic follows Espressif's reference implementations, but it has never
played a note. See [`../docs/SUBMISSION.md`](../docs/SUBMISSION.md).
