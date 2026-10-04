# Dome Light Settings

The chosen brightness for day and night. The values come from watching the
calibration sweep (black to full red) on the bench.

**Decided:** 2026-10-04

## Brightness

| Mode | Colour | Brightness | Est. current |
|---|---|---|---|
| **Day** | Red `(255, 0, 0)` | **255 / 255** (100%) | ~300 mA |
| **Night** | Red `(255, 0, 0)` | **191 / 255** (75%) | ~230 mA |

Current estimates come from FastLED's power model for all 16 LEDs and include
the board. Both fit within USB's 500 mA and within any buck converter rated
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
| Dithering | Off, so each level is a real hardware step |
| Day/night input | Photoresistor on GP15 + GP26, readings in `CALIBRATION.md` |

## Still to decide

- **Day/night threshold.** The calibration readings put dark at ~150 and a lit
  room at ~2500. The real night reading still needs to be taken with the light
  mounted.
- **How it changes.** A hard switch between 191 and 255 needs hysteresis so it
  doesn't flip back and forth near the threshold. A gradual blend between the
  two avoids that.
- **The LEDs' effect on the sensor.** On the bench, the LEDs raised the light
  reading by roughly 750–950 at full brightness. At night brightness they will
  still raise it noticeably, which could make the room read as "day". Shielding
  the sensor, or subtracting the LEDs' known contribution in software, has to
  be part of the design.
