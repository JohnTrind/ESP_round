# CrowPanel 1.28" HMI ESP32 Rotary Display — Arduino + LVGL

> 🇵🇹 [Versão em português](README.pt.md)

Rotary-menu scaffold for Elecrow's round board (240×240 IPS, capacitive touch, push-button encoder, ring of 5 WS2812 LEDs).

Hardware confirmed with `esptool` on this unit: ESP32-S3R8, 16 MB flash (quad, 3.3 V), 8 MB octal PSRAM, native USB-Serial/JTAG.

## The detail that isn't in Elecrow's wiki

**The display + touch assembly has an enable on `GPIO 1` with an external pull-down — so it boots switched off.** Unless you drive it HIGH, the screen never lights up and the touch controller doesn't answer on I²C. It's not in the product documentation; I found it by measurement (see `src/diag.cpp`).

How it was isolated:

| Test | Result |
|---|---|
| WS2812 ring on GPIO 48 | lights up → main board and power OK |
| Backlight GPIO 46 and the other 28 GPIOs, HIGH and LOW | screen dark |
| Passive pull-up map | external pull-ups on 6/7, 38/39, 41/42/45; **pull-down on 1 and 2** |
| I²C scan on 6/7 | 0 devices |
| I²C scan on 6/7 **with GPIO 1 HIGH** | CST816 at `0x15`, 5/5 ACK |

`GPIO 2` is the other pin with an external pull-down and still has no identified function — probably the enable for something else.

## Build and flash

```powershell
python -m platformio run -e crowpanel_128_rotary -t upload   # application
python -m platformio run -e diag -t upload                   # diagnostics
python -m platformio device monitor                          # serial
```

Two environment notes:

- **GFX Library is pinned to `~1.3.9`.** 1.4+ requires Arduino-ESP32 core 3.x, while PlatformIO's stable platform ships 2.0.17. The `~` matters — with `^1.3.9` semver resolves up to 1.6.9 and breaks again.
- **Opening the serial port resets the chip.** Any state set by REPL commands is gone on the next connection; anything that must persist goes in `setup()`.

## Structure

| File | Role |
|---|---|
| `include/pins.h` | pinout, including `PANEL_EN` |
| `include/lv_conf.h` | minimal LVGL (RGB565, `millis` tick, fonts) |
| `src/display.cpp` | GC9A01 + CST816D → LVGL, double draw buffers in internal RAM |
| `src/encoder.cpp` | ISR quadrature, detents and debounced click |
| `src/ui.cpp` | rotary menu and the 4 demos |
| `src/diag.cpp` | diagnostics REPL (env `diag`) |

Decisions worth the context:

- **The encoder is not an `lv_indev`.** The app reads it directly, because LVGL's encoder mode forces groups and edit state, which on a round UI adds more friction than value. Each screen decides what the knob does.
- **Touch has a 70 ms hold window.** The CST816D reports by interrupt, not continuously; without it LVGL only sees instant taps and never a drag.
- **Draw buffers are 1/6 of the screen, doubled, in internal RAM.** That's 2×19 KB, faster for SPI than PSRAM; a full framebuffer (115 KB) would also fit.

## Diagnostics (`env:diag`)

REPL over the serial port, one command per line:

```
h/l/z <pin>   pin HIGH / LOW / released
H | L | Z     all candidates HIGH / LOW / released
m <a> <b>     HIGH only within the range
d <pin> | D   drive and read the pad back (detects input-only)
p             passive map of external pull-ups/pull-downs
i | S         I²C scan (simple / robust with retries)
I             brute-force I²C over pin pairs
E             hunt for the panel enable pin
t             repeated CST816 probe
r             light the WS2812 ring
g             init the GC9A01 and paint a colour cycle
?             current state
```

Driven from outside with a small serial helper script:

```powershell
python poke.py COM11 --wait 20 "h 1" "S"
```

The I²C probe is bit-banged rather than `Wire`: the 2.0.17 core driver resets the chip when re-initialised on pins with no device. Even bit-banged, a single probe gives false positives on floating lines — so `S` and `E` use retries and only a consistent answer (5/5, 4/4) counts.

## Factory backup

Before flashing, the original Elecrow image was read out as `factory_backup.bin` (16 MB, not included in the repo). To restore:

```powershell
python -m esptool --port COM11 write-flash 0 factory_backup.bin
```

## License

[MIT](LICENSE)
