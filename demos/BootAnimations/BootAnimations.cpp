// BootAnimations - a demo reel of boot-up animation ideas for the dome light.
//
// This is NOT the dome light firmware. Flash it to look at ideas, then put the
// real firmware back with `make upload` from the project root.
//
// Every animation reaches steady full red - the light's "on" state - within
// 500ms of starting. The reel loops forever. Before each animation, dim dots
// along the top row show its number (one dot = animation 1, two dots =
// animation 2, ...). Each one plays twice, holding full red for a second after
// each play, and the time it took to reach full red is printed over serial.

#include <Arduino.h>
#include <FastLED.h>

#define DATA_PIN     6
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

#define WIDTH        8
#define HEIGHT       2
#define NUM_LEDS     (WIDTH * HEIGHT)

// Most 2x8 arrays are one strip folded back on itself: row 0 runs left to
// right and row 1 runs right to left. If the wipes look scrambled, try false.
#define SERPENTINE   true

// Safety net for USB. Everything here stays in red and amber, which peaks
// around 300mA, so this shouldn't engage. It's here in case a colour gets
// changed to something whiter while experimenting.
#define SAFETY_MILLIAMPS 450

// Set to an animation's number to loop just that one; 0 plays the whole reel.
#define FOCUS        0

CRGB leds[NUM_LEDS];
static const CRGB ON_RED = CRGB(255, 0, 0);
static const CRGB AMBER  = CRGB(255, 90, 10);

// --- Layout helpers ---------------------------------------------------------

static uint8_t XY(uint8_t x, uint8_t y) {
  if (SERPENTINE && (y & 1)) return y * WIDTH + (WIDTH - 1 - x);
  return y * WIDTH + x;
}

static void setColumn(uint8_t x, const CRGB &c) {
  for (uint8_t y = 0; y < HEIGHT; y++) leds[XY(x, y)] = c;
}

// Position k around the rectangle: along the top row left to right, then back
// along the bottom row right to left.
static uint8_t ringIndex(uint8_t k) {
  k %= NUM_LEDS;
  return (k < WIDTH) ? XY(k, 0) : XY(NUM_LEDS - 1 - k, 1);
}

static CRGB red(uint8_t level) { return CRGB(level, 0, 0); }

// 0 before `start`, 255 after `start + duration`, linear in between.
static uint8_t ramp(uint32_t t, uint32_t start, uint32_t duration) {
  if (t <= start) return 0;
  if (t >= start + duration) return 255;
  return (t - start) * 255 / duration;
}

// Calls frame(t, progress) at ~120fps for `ms` milliseconds. t is the elapsed
// time in ms; progress runs 0..255 across the duration. A short frame interval
// keeps sub-half-second motion smooth; 16 LEDs take well under 1ms to send.
template <typename Frame>
static void run(uint32_t ms, Frame frame) {
  uint32_t start = millis();
  while (true) {
    uint32_t t = millis() - start;
    if (t >= ms) break;
    frame(t, (uint8_t)(t * 255 / ms));
    FastLED.show();
    delay(8);
  }
}

// --- The animations ---------------------------------------------------------
// Each one must reach full red within 500ms. They target ~450ms so frame
// timing can't push them over; the reel measures and prints the real figure.

// 1. Ember: a quick eased fade-in with a flicker that calms as it brightens.
static void ember() {
  run(450, [](uint32_t, uint8_t p) {
    uint8_t level = ease8InOutCubic(p);
    uint8_t flicker = (255 - p) / 3;   // strong at the start, gone by the end
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      leds[i] = red(qsub8(level, random8(flicker + 1)));
    }
  });
}

// 2. Bloom: lights from the two middle columns outward to both edges. Each
// column comes in amber and deepens to red as it brightens - the same colour
// shift as Sunset - so the advancing tips of the sweep glow amber while the
// centre has already turned red.
static void bloom() {
  run(300, [](uint32_t t, uint8_t) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      uint8_t ring = (x < WIDTH / 2) ? (WIDTH / 2 - 1 - x) : (x - WIDTH / 2);  // 0 = centre
      uint8_t q = ease8InOutQuad(ramp(t, ring * 45, 165));   // this column's progress
      setColumn(x, CHSV(scale8(HUE_ORANGE, 255 - q), 255, q));   // amber -> red
    }
  });
}

// 3. Scanner: one fast sweep left to right with a trail, then the array fills.
static void scanner() {
  const uint32_t sweepMs = 300;
  run(sweepMs, [=](uint32_t t, uint8_t) {
    fadeToBlackBy(leds, NUM_LEDS, 45);
    setColumn(t * WIDTH / sweepMs, ON_RED);
  });

  static CRGB from[NUM_LEDS];
  for (uint8_t i = 0; i < NUM_LEDS; i++) from[i] = leds[i];
  run(150, [](uint32_t, uint8_t p) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) leds[i] = blend(from[i], ON_RED, ease8InOutQuad(p));
  });
}

// 4. Sparks: pixels ignite in random order in a quick crackle, each with an
// amber pop that settles to red.
static void sparks() {
  static uint8_t order[NUM_LEDS];
  for (uint8_t i = 0; i < NUM_LEDS; i++) order[i] = i;
  for (uint8_t i = NUM_LEDS - 1; i > 0; i--) {      // shuffle
    uint8_t j = random8(i + 1);
    uint8_t tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }

  const uint32_t gap = 22, settle = 90;              // 16 x 22 + 90 = 442ms
  run(NUM_LEDS * gap + settle, [=](uint32_t t, uint8_t) {
    for (uint8_t k = 0; k < NUM_LEDS; k++) {
      uint32_t litAt = k * gap;
      CRGB c = CRGB::Black;
      if (t >= litAt) {
        c = blend(AMBER, ON_RED, ramp(t, litAt, settle));   // pop, then settle
      }
      leds[order[k]] = c;
    }
  });
}

// 5. Heartbeat: one quick "lub-dub", then straight up to full.
static uint8_t pulse(uint32_t t, uint32_t centre, uint32_t halfWidth, uint8_t peak) {
  uint32_t d = (t > centre) ? t - centre : centre - t;
  if (d >= halfWidth) return 0;
  uint8_t shape = 255 - d * 255 / halfWidth;
  return scale8(ease8InOutQuad(shape), peak);
}

static void heartbeat() {
  run(450, [](uint32_t t, uint8_t) {
    uint8_t beat = qadd8(pulse(t, 60, 50, 140), pulse(t, 170, 60, 255));
    uint8_t rise = ease8InOutCubic(ramp(t, 250, 200));
    fill_solid(leds, NUM_LEDS, red(beat > rise ? beat : rise));
  });
}

// 6. Progress bar: columns fill left to right. The leading column glows amber
// as it fills, then turns red once the bar has passed it.
static void progressBar() {
  run(450, [](uint32_t, uint8_t p) {
    uint32_t pos = (uint32_t)p * WIDTH * 16 / 255;   // in 1/16ths of a column
    for (uint8_t x = 0; x < WIDTH; x++) {
      uint32_t colStart = x * 16;
      CRGB c = CRGB::Black;
      if (pos >= colStart + 16) {
        c = ON_RED;
      } else if (pos > colStart) {
        c = blend(CRGB::Black, AMBER, (uint8_t)((pos - colStart) * 16));
      }
      setColumn(x, c);
    }
  });
}

// 7. Sunset: a dim amber glow that deepens into red as it brightens.
static void sunset() {
  run(450, [](uint32_t, uint8_t p) {
    uint8_t hue = scale8(HUE_ORANGE, 255 - p);   // orange -> red
    uint8_t val = ease8InOutCubic(p);
    uint8_t shimmer = (255 - p) / 6;
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(hue, 255, qsub8(val, random8(shimmer + 1)));
    }
  });
}

// 8. Orbit: an amber head races once around the rectangle, leaving every pixel
// it passes lit red.
static void orbit() {
  const uint32_t lapMs = 420;
  run(lapMs, [=](uint32_t t, uint8_t) {
    uint8_t head = t * NUM_LEDS / lapMs;
    for (uint8_t k = 0; k < NUM_LEDS; k++) {
      leds[ringIndex(k)] = (k < head) ? ON_RED : (k == head ? AMBER : CRGB::Black);
    }
  });
}

struct Animation {
  const char *name;
  void (*play)();
};

static const Animation ANIMATIONS[] = {
  {"Ember",        ember},
  {"Bloom",        bloom},
  {"Scanner",      scanner},
  {"Sparks",       sparks},
  {"Heartbeat",    heartbeat},
  {"Progress bar", progressBar},
  {"Sunset",       sunset},
  {"Orbit",        orbit},
};
static const uint8_t NUM_ANIMATIONS = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);

// --- Reel -------------------------------------------------------------------

// n dim dots along the top row, so you can tell which animation is next.
static void showNumber(uint8_t n) {
  FastLED.clear();
  for (uint8_t x = 0; x < n && x < WIDTH; x++) leds[XY(x, 0)] = red(25);
  FastLED.show();
  delay(1000);
  FastLED.clear(true);
  delay(400);
}

// Hold the finished "on" state, then fade out.
static void holdAndFade() {
  delay(1000);
  run(400, [](uint32_t, uint8_t p) { fill_solid(leds, NUM_LEDS, red(255 - p)); });
  FastLED.clear(true);
  delay(600);
}

// Play one animation, force full red, and report how long it took to get
// there. The reel's rule: at most 500ms.
static uint32_t playTimed(const Animation &a) {
  uint32_t start = millis();
  a.play();
  fill_solid(leds, NUM_LEDS, ON_RED);
  FastLED.show();
  return millis() - start;
}

void setup() {
  Serial.begin(115200);
  random16_add_entropy(micros());

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, SAFETY_MILLIAMPS);
  FastLED.setDither(DISABLE_DITHER);
  FastLED.clear(true);
}

void loop() {
  for (uint8_t i = 0; i < NUM_ANIMATIONS; i++) {
    if (FOCUS && i != FOCUS - 1) continue;
    if (!FOCUS) showNumber(i + 1);
    // Twice in a row in the full reel: at under half a second, one viewing is
    // easy to miss. When focused on one animation it just repeats anyway.
    for (uint8_t pass = 1; pass <= (FOCUS ? 1 : 2); pass++) {
      uint32_t ms = playTimed(ANIMATIONS[i]);
      Serial.printf("%u/%u %-12s pass %u: full red at %lu ms\n",
                    i + 1, NUM_ANIMATIONS, ANIMATIONS[i].name, pass, (unsigned long)ms);
      holdAndFade();
    }
  }
}
