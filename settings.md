# Dome Light Settings

The chosen brightness for day and night. The values come from watching the
calibration sweep (black to full red) on the bench.

**Decided:** 2026-10-04

## Brightness

| Mode | Colour | Brightness | Est. current |
|---|---|---|---|
| **Day** | Amber `(255, 90, 10)` | **255 / 255** (100%) | ~350 mA |
| **Night** | Red `(255, 0, 0)` | **191 / 255** (75%) | ~230 mA |

Current estimates come from FastLED's power model for all 16 LEDs and include
the board.

Day was changed from deep red to amber on 2026-10-04, after previewing solid
amber on the array. It's the same amber used in the boot animation demos.
Amber draws a little more than red because of the green channel.

## How the mode is chosen

At power-up, before any LED is lit, the firmware reads the photoresistor once:

- **Below 500 → night.** 500 or more → day.
- The mode then holds until the next power-up. The light never switches mid-session.

The sensor sits behind tinted plastic, which lowers all readings; 500 leaves a
wide margin above the night reading (~70). Reading only at
boot avoids the sensor seeing the dome light's own glow: with the LEDs on, a
reading of 932 rose to ~1460.

## Boot animation

**Bloom snap**, played once on power-up in the chosen mode's colour and
brightness: pairs of columns switch straight from off to full, centre pair
first and working out to the ends, one pair every **37 ms**. Fully lit at
~111 ms, then steady. No fading, no colour change. Both fit within USB's 500 mA and within any buck converter rated
1 A or more.

### Note on 75%

75% is three quarters of the hardware brightness value (191 of 255). Eyes
respond to light non-linearly, so night mode will look only somewhat dimmer
than day, not a quarter dimmer. If night should look clearly dimmer, the value
needs to be lower; check it on the real LEDs before changing it.

## Related settings

| Setting | Value |
|---|---|
| LEDs | 2x8 WS2812B, GRB, data on GP6 |
| Power cap | None (removed) |
| Dithering | Off, so night's 75% is one steady hardware level |
| Day/night input | Photoresistor on GP15 + GP26, readings in `CALIBRATION.md` |

## Still to decide

- **Whether 500 holds once mounted.** The night reading (~70) and the day
  readings were taken on the bench. Check the boot readings in the final
  position, in daylight and at night: set `DEBUG` to `true` in
  `src/DomeLight.cpp`, reflash, and watch `make monitor`.
