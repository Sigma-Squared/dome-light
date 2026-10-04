# Dome Light

FastLED on a Waveshare RP2040-Zero driving a 2x8 WS2812B array (16 LEDs) on
GP6, with a photoresistor for sensing room light.

The plan is for the light to follow the room: full red during the day and
dimmer at night. That isn't built yet. For now the firmware runs a
**calibration sweep** and reports the light sensor, which is how the day and
night levels in [`settings.md`](settings.md) were chosen.

```
dome-light-final/
├── Makefile             # compile / upload / uf2 / monitor
├── README.md
├── CALIBRATION.md       # photoresistor readings: dark, room light, flashlight
├── settings.md          # chosen day/night brightness and what's still open
├── .vscode/             # IntelliSense + build tasks
└── src/
    ├── DomeLight.cpp    # the code
    ├── src.ino          # required entry point, named after the folder (empty)
    └── sketch.yaml      # pinned core + library versions
```

## Wiring

| Part | RP2040-Zero |
|---|---|
| Array DIN | **GP6** (pad 17, right edge), through a 330–470 Ω resistor |
| Array GND | GND (always shared, whatever powers the array) |
| Array 5V | see *Power* below |
| Photoresistor | across **GP15** and **GP26** (pads 8 and 7), no other parts |

## Current firmware: calibration sweep

The LEDs ramp from completely dark up to full red and back down on a
30-second loop: 12 s up, 3 s at full, 12 s down, 3 s dark. Twice a second it
prints the brightness and the light reading over USB serial (`make monitor`):

```
brightness 191/255 | light 2381/4095
```

- **The ramp is squared, not linear.** Eyes notice changes at the dim end far
  more than near full, so a linear ramp would seem to jump out of darkness and
  then look "about full" for most of the sweep.
- **Dithering is off.** Each step is a real hardware level rather than
  FastLED flickering between levels to fake in-between values.
- **There is no power cap.** Full red is ~300 mA by FastLED's estimate, within
  a USB port's 500 mA. Nothing limits current, so **don't switch to white or
  other bright mixed colours while on USB**: full white is ~710 mA or more, and
  the board has no fuse.

## The photoresistor

GP15 is driven HIGH as the photoresistor's 3.3V supply. GP26 reads the voltage
across the RP2040's internal pull-down, which stands in for the fixed resistor
of a normal voltage divider. Brighter light gives a higher reading (0–4095).

It's read through the Pico SDK (`adc_gpio_init`, then `gpio_pull_down`) rather
than `analogRead()`, because `analogRead()` turns the pull-down off the first
time it reads a pin and the input then floats. Don't add `analogRead()` calls
to this sketch.

Bench readings are in [`CALIBRATION.md`](CALIBRATION.md): about 150 covered,
about 2500 in a lit room. The internal pull-down varies from chip to chip, so
those numbers apply to this board only.

**The sensor picks up the LEDs.** On the bench, the reading rose by roughly
750–950 between LEDs off and full brightness. For a light that brightens with
the room, that's a feedback loop: at night its own glow can read as daytime
and keep it bright. Shield the sensor from the LEDs when mounting, and plan to
compensate in software as a backup.

## Power

### Now: from the board's 5V pad, on USB

Fine for red: full red is about 300 mA by FastLED's estimate.

### Once mounted: buck converter

A buck rated **1 A or more** runs every colour at full brightness. Full white
is ~710 mA by FastLED's model, and some WS2812B batches draw closer to 1 A.

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

Full white is about 3.5 W of LEDs in an enclosed dome, so give it some airflow
if you run bright white for long periods. Full red is about 1.5 W.

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
**BOOT** while plugging it in and run `make uf2` instead. To watch the sweep and
the light readings:

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
