// DomeLight - 2x8 WS2812B array on a Waveshare RP2040-Zero.
//
// At power-up, before any LED is lit, it reads the photoresistor once and
// picks a mode for the whole session:
//
//   Day   (reading >= NIGHT_THRESHOLD): amber at full brightness
//   Night (reading <  NIGHT_THRESHOLD): red at 75%
//
// It then plays the boot animation (Bloom snap) straight into that mode's
// colour and brightness, and stays there. Reading only at boot, with the LEDs
// still off, keeps the decision consistent: the sensor can see the dome light
// itself, so a reading taken with the LEDs on would be skewed by their glow.
//
// NO POWER CAP. Day amber is ~350mA by FastLED's estimate, within a USB port's
// 500mA. Nothing limits current, so don't switch to a whiter colour while on
// USB: full white is ~710mA or more.
//
// Wiring:
//   Array DIN -> GP6 (pad 17, right edge), ideally through a 330-470 ohm
//                resistor. GP6 is 3.3V logic; see the README if the first
//                pixel flickers or shows the wrong colour.
//   Array GND -> board GND (always, whatever powers the array)
//   Array 5V  -> board 5V pad while on USB; a buck converter once mounted.
//                Read the README's power section before switching over.
//   Photoresistor across GP15 and GP26 (pads 8 and 7), no other parts. It sits
//   behind tinted plastic, which is why the night threshold is as high as 500.

#include <Arduino.h>
#include <FastLED.h>
#include <hardware/adc.h>
#include <hardware/gpio.h>

// Serial debug output: the mode, the boot reading it was based on, and the live
// light reading, once a second over USB (`make monitor`). When false, none of
// the serial code is compiled in.
#define DEBUG          false

#define DATA_PIN       6
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

#define ARRAY_WIDTH    8
#define ARRAY_HEIGHT   2
#define NUM_LEDS       (ARRAY_WIDTH * ARRAY_HEIGHT)

// Day/night decision, from the photoresistor reading at boot (0..4095).
// Below this is night. See CALIBRATION.md for the readings it's based on.
#define NIGHT_THRESHOLD   500

// The two modes. See settings.md.
static const CRGB DAY_COLOR        = CRGB(255, 90, 10);   // amber
static const uint8_t DAY_BRIGHTNESS   = 255;              // 100%
static const CRGB NIGHT_COLOR      = CRGB(255, 0, 0);     // red
static const uint8_t NIGHT_BRIGHTNESS = 191;              // 75%

// Boot animation timing: the gap between each pair of columns switching on.
// Four steps (the centre pair, then three more pairs out to the ends), so the
// array is fully lit at 3 x this.
#define BOOT_STEP_MS   37

// Photoresistor. GP15 is driven HIGH as its 3.3V supply; GP26 (analog channel
// 0) reads the voltage across the chip's internal pull-down, which stands in
// for the fixed resistor of a normal divider. Brighter light = higher reading.
#define LDR_POWER_PIN  15
#define LDR_SENSE_PIN  26

CRGB leds[NUM_LEDS];

// Decided once at boot.
static bool     gNight = false;
static uint16_t gBootReading = 0;
static CRGB     gColor;

// Most 2x8 arrays are one strip folded back on itself: row 0 runs left to
// right and row 1 runs right to left. The boot animation is symmetric about
// the middle, so it looks the same even if this assumption is wrong.
static uint8_t XY(uint8_t x, uint8_t y) {
  if (y & 1) return y * ARRAY_WIDTH + (ARRAY_WIDTH - 1 - x);
  return y * ARRAY_WIDTH + x;
}

static void setColumn(uint8_t x, const CRGB &c) {
  for (uint8_t y = 0; y < ARRAY_HEIGHT; y++) leds[XY(x, y)] = c;
}

// Column distance from the middle: 0 for the two centre columns, 3 at the ends.
static uint8_t centreRing(uint8_t x) {
  return (x < ARRAY_WIDTH / 2) ? (ARRAY_WIDTH / 2 - 1 - x) : (x - ARRAY_WIDTH / 2);
}

// Boot animation: Bloom snap. Pairs of columns switch straight from off to the
// mode's colour at full mode brightness, centre first and working outward,
// one pair every BOOT_STEP_MS. No fading and no colour change along the way.
static void bootAnimation(const CRGB &color) {
  const uint8_t steps = ARRAY_WIDTH / 2;
  for (uint8_t ring = 0; ring < steps; ring++) {
    for (uint8_t x = 0; x < ARRAY_WIDTH; x++) {
      setColumn(x, (centreRing(x) <= ring) ? color : CRGB::Black);
    }
    FastLED.show();
    if (ring < steps - 1) delay(BOOT_STEP_MS);
  }
}

// Read through the Pico SDK rather than analogRead(): analogRead() turns the
// pin's pull-down off the first time it reads a pin, and without it the input
// floats. Don't mix analogRead() into this sketch for the same reason.
static void setupLightSensor() {
  pinMode(LDR_POWER_PIN, OUTPUT);
  digitalWrite(LDR_POWER_PIN, HIGH);

  adc_init();
  adc_gpio_init(LDR_SENSE_PIN);    // analog mode (this also turns the pulls off)...
  gpio_pull_down(LDR_SENSE_PIN);   // ...so turn the pull-down back on afterwards
}

// 0..4095. Averages 16 samples to smooth out ADC noise.
static uint16_t readLight() {
  adc_select_input(LDR_SENSE_PIN - 26);
  uint32_t sum = 0;
  for (uint8_t i = 0; i < 16; i++) {
    sum += adc_read();
  }
  return sum / 16;
}

void setup() {
  // 1. Read the room while the LEDs are still off.
  setupLightSensor();
  gBootReading = readLight();
  gNight = gBootReading < NIGHT_THRESHOLD;
  gColor = gNight ? NIGHT_COLOR : DAY_COLOR;

  // 2. Light up in that mode.
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(gNight ? NIGHT_BRIGHTNESS : DAY_BRIGHTNESS);
  // Show the 75% night level as one steady hardware level. With dithering on,
  // FastLED flickers between neighbouring levels to fake in-between values.
  FastLED.setDither(DISABLE_DITHER);
  bootAnimation(gColor);

#if DEBUG
  Serial.begin(115200);
#endif
}

void loop() {
  // Steady in the boot-time mode. Re-sending the frame is cheap, and it means
  // the LEDs recover on their own if the array is powered up after the board.
  fill_solid(leds, NUM_LEDS, gColor);
  FastLED.show();
  delay(20);

#if DEBUG
  // Report the mode and what it was based on, plus the live reading for
  // reference. The live reading doesn't change the mode.
  EVERY_N_MILLISECONDS(1000) {
    Serial.printf("mode %s (boot reading %u, threshold %u) | light now %u\n",
                  gNight ? "NIGHT" : "DAY", gBootReading, NIGHT_THRESHOLD, readLight());
  }
#endif
}
