# Photoresistor Calibration

Readings from the photoresistor, used to choose thresholds for how the light
responds to the room.

**Date:** 2026-10-04
**Location:** bench test, sensor unmounted

## Setup

| | |
|---|---|
| Board | Waveshare RP2040-Zero |
| Sensor | Photoresistor across GP15 (driven HIGH) and GP26 (ADC0) |
| Fixed resistor | RP2040 internal pull-down on GP26 (nominally ~50–80 kΩ, varies per chip) |
| Reading | 12-bit, 0–4095, average of 16 samples, reported every 0.5 s over USB serial |
| Firmware | `src/DomeLight.cpp` with `readLight()`; LEDs dim red at a 25 mA cap during all tests |

Higher readings mean more light. Because the pull-down's value varies from
chip to chip, these numbers apply to **this board**. Re-run the tests if the
board is replaced.

## Results

| Condition | Readings | Typical |
|---|---|---|
| Covered (simulated night) | 128–189 | **~150** |
| Room light, early session | 2111–2354 | ~2150 |
| Room light, during tests | 2495–2519 | **~2510** |
| Room light, end of tests | 2624–2692 | ~2660 |
| Phone flashlight at ~1 cm | 4037–4053 | **~4045** |

### Recommended working range

- **Dark:** ~150
- **Lit room:** ~2500
- Treat anything above ~2500 as fully bright. The flashlight reading is far
  brighter than real indoor daylight, so it isn't a useful "day" point.

## Observations

- **Wide separation.** Dark and lit room are about 2350 apart, so telling them
  apart is reliable.
- **Room light varies.** It read anywhere from ~2110 to ~2690 over the session,
  with no deliberate changes to the lighting. Thresholds should leave room for
  that.
- **Asymmetric response.** After being covered, the reading took several
  seconds to settle and was still creeping down after 8 s. After being
  uncovered, it recovered within about 0.5 s. This is normal for
  photoresistors.
- **Flashlight aim matters.** The first flashlight attempt, held a few
  centimetres away, read only ~2610, barely above room light. Held at ~1 cm
  and pointed straight at the sensor, it read ~4045.

## Still to do

- **Take a real night reading once mounted.** Covering the sensor gives a true
  zero. A real room at night will likely read higher because of the dome
  light's own glow, streetlights, or standby LEDs. Measure in the final
  position, with the LEDs on, before fixing the "dark" threshold.
- **Re-check room light in the mounted position.** Angle and placement will
  change it.

## Raw captures

Each value is one 0.5 s report, in order.

**Room light, early session (6 s):**
2226 2242 2269 2354 2327 2118 2120 2140 2156 2141 2115 2111

**Room light, ambient baseline (6 s):**
2519 2514 2508 2506 2500 2515 2514 2516 2495 2498 2499 2515

**Night test (15 s).** Partly covered at first; fully covered from ~6.5 s.
816 800 1041 1111 1125 1112 1103 1101 1079 1156 1212 1100 859 |
308 285 266 240 204 184 186 163 154 178 189 174 186 179 156 147 143

**Day test, take 1 (15 s).** Still covered for the first ~1.3 s; flashlight
not aimed at the sensor. Not used.
142 136 128 | 744 1717 2104 2364 2482 2523 2530 2585 2715 2756 2813 2633 2544
2599 2811 2664 2647 2610 2615 2625 2579 2553 2577 2603 2605 2587 2171

**Day test, take 2 (15 s).** Flashlight at ~1 cm for the first ~1 s, then off
the sensor.
4037 4045 4053 | 2969 3802 3684 3927 | 2760 2740 2683 2635 2624 2630 2653 2662
2651 2639 2642 2661 2670 2659 2644 2649 2661 2676 2668 2657 2660 2677 2692
