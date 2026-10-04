# Dome Light

FastLED on a Waveshare RP2040-Zero driving a 2x8 WS2812B array (16 LEDs) on
GP6. Right now it lights every LED dim red.

```
dome-light-final/
├── Makefile             # compile / upload / uf2 / monitor
├── README.md
├── .vscode/             # IntelliSense + build tasks
└── src/
    ├── DomeLight.cpp    # the code
    ├── src.ino          # required entry point, named after the folder (empty)
    └── sketch.yaml      # pinned core + library versions
```

## Wiring

| Array | RP2040-Zero |
|---|---|
| DIN | **GP6** (pad 17, right edge), through a 330–470 Ω resistor |
| GND | GND (always shared, whatever powers the array) |
| 5V | see *Power* below |

## How the dimness works

Brightness is set to 255. The dimming comes **only** from
`FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS)`: at every `show()`,
FastLED estimates the current draw and scales brightness down to fit the budget.

FastLED's default model counts a full-red pixel as 16 mA, an unlit pixel as
1 mA, and the MCU as a flat 25 mA. That makes 16 pixels at full red ~296 mA, so:

| `MAX_MILLIAMPS` | Effective brightness |
|---|---|
| 12 | ≈10/255 |
| **25** (current) | **21/255** — confirmed on hardware via serial |
| 50 | ≈43/255 |

Two things follow from this being a *power* cap rather than a brightness
setting:

- **Changing the colour changes the brightness.** White draws about 2.4× what
  red does, so switching to white under the same cap makes it about 2.4× dimmer.
  That's the limiter doing its job.
- **The cap is an estimate, not a measured limit.** The formula scales the
  unlit-pixel and MCU terms along with brightness, even though real hardware
  keeps drawing them. Expect actual draw a bit above the cap: roughly 35–40 mA
  for the array at 25 mA, plus the board itself.

`make monitor` prints the brightness the limiter chose every two seconds.

## Power

### Now: from the board's 5V pad, on USB

Fine at this cap. Even with no cap at all, 16 LEDs at full white are only
~690 mA by FastLED's model.

### Once mounted: buck converter

**The RP2040-Zero has no diode between USB VBUS and its 5V pad** — the
schematic wires them straight together. If the buck feeds the 5V pad and you
plug in USB to reflash, the buck and your computer's USB port are connected
directly. Pick one of these:

1. **Buck powers the array only.** The board stays on USB, and only GND and DIN
   cross between the two. Simplest, and nothing can backfeed.
2. **Buck powers both, through a Schottky diode** into the 5V pad. Then plugging
   in USB is safe.
3. **Buck powers both, and you disconnect it before plugging in USB.** Works,
   but it relies on you remembering every time.

Size `MAX_MILLIAMPS` comfortably below the buck's rating if you raise it.

### Logic level, and a buck-converter trick

GP6 outputs 3.3V. A WS2812B at 5V wants about 3.5V for a logic high, so this is
slightly out of spec. It often works anyway, and the usual symptom when it
doesn't is a flickering or wrong-coloured first pixel.

Since you're adding a buck converter anyway, the easy fix is to **set it to
about 4.6V instead of 5V**. The input threshold scales with supply voltage
(0.7 × 4.6V ≈ 3.2V), which puts GP6's 3.3V back in spec. WS2812B is rated down
to 3.5V, so the only cost is slightly lower maximum brightness. The
alternative is a 74AHCT125 level shifter. It has to be **AHCT** or HCT: a plain
74HC125 at 5V has the same 3.5V input threshold and fixes nothing.

## Build and upload

```bash
make upload
```

The port is auto-detected. If the board doesn't show up as a serial port, hold
**BOOT** while plugging it in and run `make uf2` instead. To watch the limiter's
output:

```bash
make monitor
```

VS Code: open this folder as the workspace root. `Cmd+Shift+B` compiles; upload
and monitor are under *Terminal → Run Task*. The IntelliSense paths are pinned
to core 6.0.0.

## Versions

- arduino-cli 1.5.1
- rp2040:rp2040 (arduino-pico) 6.0.0
- FastLED 3.10.5
