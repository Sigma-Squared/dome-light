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
//
// The reel is built around three favourites - Bloom (#2), Sparks (#4) and
// Progress bar (#6) - with the other five as variants of them. They share one
// idea: light travels across the array, arriving amber and settling to red.

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

// Which animations to play, by number, in the order listed. Use { 0 } to play
// all of them. With a single entry, the number dots are skipped and it simply
// repeats.
static const uint8_t PLAYLIST[] = { 9 };

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

// Column distance from the middle: 0 for the two centre columns, 3 at the ends.
static uint8_t centreRing(uint8_t x) {
  return (x < WIDTH / 2) ? (WIDTH / 2 - 1 - x) : (x - WIDTH / 2);
}

// Column distance from the nearest end: 0 at the ends, 3 in the middle.
static uint8_t edgeRing(uint8_t x) {
  return (WIDTH / 2 - 1) - centreRing(x);
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

// --- The three looks the reel is built from -----------------------------------

// Bloom's look: fading in from black, starting amber and deepening to red as it
// brightens (the colour shift Sunset used). q is 0..255 progress.
static CRGB warm(uint8_t q) {
  return CHSV(scale8(HUE_ORANGE, 255 - q), 255, q);
}

// Sparks' look: black until `litAt`, then an instant amber pop that settles to
// red over `settle` ms.
static CRGB sparkAt(uint32_t t, uint32_t litAt, uint32_t settle) {
  if (t < litAt) return CRGB::Black;
  return blend(AMBER, ON_RED, ramp(t, litAt, settle));
}

// Progress bar's look for one column. `pos` is how far the bar has travelled
// and `colStart` where this column begins, both in 1/16ths of a column. Red
// once the bar has passed, glowing amber while the bar is inside it, black
// before.
static CRGB progressCell(int32_t pos, int32_t colStart) {
  if (pos >= colStart + 16) return ON_RED;
  if (pos > colStart) return blend(CRGB::Black, AMBER, (uint8_t)((pos - colStart) * 16));
  return CRGB::Black;
}

// --- The animations ---------------------------------------------------------
// Each one must reach full red within 500ms. They target at most ~450ms so
// frame timing can't push them over; the reel measures and prints the real
// figure.

// 1. Converge (Bloom variant): Bloom in reverse - lights from both ends inward,
// the amber tips meeting in the middle.
static void converge() {
  run(300, [](uint32_t t, uint8_t) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      setColumn(x, warm(ease8InOutQuad(ramp(t, edgeRing(x) * 45, 165))));
    }
  });
}

// 2. Bloom: lights from the two middle columns outward to both edges, each
// column arriving amber and deepening to red.
static void bloom() {
  run(300, [](uint32_t t, uint8_t) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      setColumn(x, warm(ease8InOutQuad(ramp(t, centreRing(x) * 45, 165))));
    }
  });
}

// 3. Spark bloom (Sparks + Bloom): sparks that spread from the middle outward.
// Each pixel's turn comes from its distance to the centre plus a random
// jitter, so it reads as a crackle that blooms.
static void sparkBloom() {
  static uint16_t litAt[NUM_LEDS];
  for (uint8_t x = 0; x < WIDTH; x++) {
    for (uint8_t y = 0; y < HEIGHT; y++) {
      litAt[XY(x, y)] = centreRing(x) * 70 + random8(61);   // latest: 270ms
    }
  }
  run(400, [](uint32_t t, uint8_t) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) leds[i] = sparkAt(t, litAt[i], 120);
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
      leds[order[k]] = sparkAt(t, k * gap, settle);
    }
  });
}

// 5. Spark sweep (Sparks + Progress bar): sparks that sweep left to right.
// Each pixel's turn comes from its column plus a random jitter, so a ragged,
// crackling front crosses the array.
static void sparkSweep() {
  static uint16_t litAt[NUM_LEDS];
  for (uint8_t x = 0; x < WIDTH; x++) {
    for (uint8_t y = 0; y < HEIGHT; y++) {
      litAt[XY(x, y)] = x * 35 + random8(71);   // latest: 315ms
    }
  }
  run(440, [](uint32_t t, uint8_t) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) leds[i] = sparkAt(t, litAt[i], 120);
  });
}

// 6. Progress bar: columns fill left to right. The leading column glows amber
// as it fills, then turns red once the bar has passed it.
static void progressBar() {
  run(450, [](uint32_t, uint8_t p) {
    int32_t pos = (int32_t)p * WIDTH * 16 / 255;
    for (uint8_t x = 0; x < WIDTH; x++) setColumn(x, progressCell(pos, x * 16));
  });
}

// 7. Split bars (Progress bar variant): two bars racing in opposite directions
// - the top row fills left to right while the bottom row fills right to left,
// their amber edges crossing in the middle.
static void splitBars() {
  run(400, [](uint32_t, uint8_t p) {
    int32_t pos = (int32_t)p * WIDTH * 16 / 255;
    for (uint8_t x = 0; x < WIDTH; x++) {
      leds[XY(x, 0)] = progressCell(pos, x * 16);
      leds[XY(x, 1)] = progressCell(pos, (WIDTH - 1 - x) * 16);
    }
  });
}

// 8. Slant wipe (Progress bar variant): a left-to-right fill where the top row
// leads the bottom by a column and a half, so the amber front is a diagonal.
static void slantWipe() {
  const int32_t lag = 24;                          // 1.5 columns, in 1/16ths
  const int32_t travel = WIDTH * 16 + lag;         // until the bottom row is done
  run(450, [=](uint32_t, uint8_t p) {
    int32_t pos = (int32_t)p * travel / 255;
    for (uint8_t x = 0; x < WIDTH; x++) {
      leds[XY(x, 0)] = progressCell(pos, x * 16);
      leds[XY(x, 1)] = progressCell(pos - lag, x * 16);
    }
  });
}

// 9. Bloom snap (Bloom variant): Bloom's middle-out order with no fading and
// no colour change. Each pair of columns switches straight from off to full
// red, one pair every 37ms, starting with the centre.
static void bloomSnap() {
  const uint32_t step = 37;
  run(3 * step, [=](uint32_t t, uint8_t) {
    for (uint8_t x = 0; x < WIDTH; x++) {
      setColumn(x, (t >= centreRing(x) * step) ? ON_RED : CRGB::Black);
    }
  });
  // The last pair (the ends) switches on as this returns: playTimed() sets
  // full red at 3 x step.
}

struct Animation {
  const char *name;
  void (*play)();
};

static const Animation ANIMATIONS[] = {
  {"Converge",     converge},
  {"Bloom",        bloom},
  {"Spark bloom",  sparkBloom},
  {"Sparks",       sparks},
  {"Spark sweep",  sparkSweep},
  {"Progress bar", progressBar},
  {"Split bars",   splitBars},
  {"Slant wipe",   slantWipe},
  {"Bloom snap",   bloomSnap},
};
static const uint8_t NUM_ANIMATIONS = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);

// --- Reel -------------------------------------------------------------------

// n dim dots, so you can tell which animation is next: the top row counts
// 1-8, and the bottom row continues from 9.
static void showNumber(uint8_t n) {
  FastLED.clear();
  for (uint8_t k = 0; k < n && k < NUM_LEDS; k++) leds[XY(k % WIDTH, k / WIDTH)] = red(25);
  FastLED.show();
  delay(1000);
  FastLED.clear(true);
  delay(400);
}

// Hold the finished "on" state, then switch off - instantly, so the reel's own
// transition never looks like part of an animation.
static void holdAndFade() {
  delay(1000);
  FastLED.clear(true);
  delay(800);
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

// Show the animation's number, then play it twice: at under half a second,
// one viewing is easy to miss. `solo` (a one-entry playlist) skips the number
// and plays once, since it's about to repeat anyway.
static void playEntry(uint8_t number, bool solo) {
  if (number < 1 || number > NUM_ANIMATIONS) return;
  const Animation &a = ANIMATIONS[number - 1];
  if (!solo) showNumber(number);
  for (uint8_t pass = 1; pass <= (solo ? 1 : 2); pass++) {
    uint32_t ms = playTimed(a);
    Serial.printf("%u/%u %-12s pass %u: full red at %lu ms\n",
                  number, NUM_ANIMATIONS, a.name, pass, (unsigned long)ms);
    holdAndFade();
  }
}

void loop() {
  const uint8_t count = sizeof(PLAYLIST) / sizeof(PLAYLIST[0]);
  if (count == 1 && PLAYLIST[0] == 0) {
    for (uint8_t n = 1; n <= NUM_ANIMATIONS; n++) playEntry(n, false);
  } else {
    for (uint8_t i = 0; i < count; i++) playEntry(PLAYLIST[i], count == 1);
  }
}
