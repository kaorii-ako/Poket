# Poket — product context

> Written from the maintainer's explicit brief (the four scoping answers on the
> firmware request) rather than a fresh interview. Anything not covered by that
> brief is marked **[assumed]** and should be corrected rather than trusted.

## What it is

A pocket MP3 player built from scratch: custom 4-layer PCB around an
ESP32-S3-WROOM-1, 3D-printed case, microSD library, 0.96" mono OLED, rotary
encoder and three transport buttons. Audio leaves either over Bluetooth A2DP
to headphones or out a 3.5 mm jack.

It is a hardware project first. The web app exists because the device has no
other way to receive files.

## Who uses it

One person: whoever built the unit. There is no fleet, no accounts, no
multi-tenant anything. **[assumed]** they are technically confident — they
soldered a QFN — but are using this on a phone, one-handed, while the device
sits on a desk.

## The web app's job

The device has no keyboard, no file manager, and a 128×64 screen. Everything
that is painful on that screen moves to the browser:

1. **Get MP3s onto the card.** The primary job. Drag, drop, done.
2. **Browse and organise** what is already there; build playlists.
3. **Choose the OLED theme**, with a preview accurate enough to decide from.
4. **Drive playback** from the browser while the device is across the room.
5. **Settings** — Wi-Fi, Bluetooth pairing, sleep, brightness, battery.

## Hard constraints that shape the design

- **Served off an ESP32-S3.** The whole app — markup, styles, script — is
  embedded in flash and gzipped. Budget: **under 60 kB compressed, single
  file, zero network dependencies.** No framework, no CDN font, no icon
  package. This is the constraint that decides the most.
- **Wi-Fi and Bluetooth share one radio.** When the web app is reachable,
  Bluetooth audio is not. Transfer mode and playback mode are genuinely
  exclusive and the UI has to be honest about that rather than hide it.
- **Uploads are slow.** ~1–2 MB/s over the device's own AP. A 60 MB album is
  most of a minute. Progress has to be real, per-file, and resumable in the
  sense that a failure names the file that failed.
- **The device may be on battery.** Long transfers matter; the UI should show
  charge state without being asked.
- **One client at a time, realistically.** No collaboration, no conflict
  resolution, no optimistic concurrency.

## What success looks like

Someone plugs in a fresh card, joins the Poket hotspot, drags a folder of
music in, picks a theme, and unplugs — without reading anything.

## Explicitly out of scope

Accounts, cloud sync, streaming services, transcoding, album art fetching,
mobile apps, OTA firmware from the browser **[assumed]** — the partition table
reserves OTA slots but nothing in the brief asked for browser-driven updates.
