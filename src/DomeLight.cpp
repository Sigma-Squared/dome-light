// DomeLight - 2x8 WS2812B array on a Waveshare RP2040-Zero.
//
// CALIBRATION SWEEP. The LEDs ramp slowly from completely dark up to full
// red and back down, so you can pick the levels that look right for night and
// for day. Each level is printed over USB serial (`make monitor`), together
// with the photoresistor reading.
//
// NO POWER CAP. Full red is ~300mA by FastLED's estimate, which is within a
// USB port's 500mA. Nothing limits current any more, so don't switch to a
// whiter colour while on USB: full white is ~710mA or more.
//
// Wiring:
//   Array DIN -> GP6 (pad 17, right edge), ideally through a 330-470 ohm
//                resistor. GP6 is 3.3V logic; see the README if the first
//                pixel flickers or shows the wrong colour.
//   Array GND -> board GND (always, whatever powers the array)
//   Array 5V  -> board 5V pad while on USB; a buck converter once mounted.
//                Read the README's power section before switching over.
//   Photoresistor across GP15 and GP26 (pads 8 and 7), no other parts.

#include <Arduino.h>
#include <FastLED.h>
#include <hardware/adc.h>
#include <hardware/gpio.h>

#define DATA_PIN       6
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

#define ARRAY_WIDTH    8
#define ARRAY_HEIGHT   2
#define NUM_LEDS       (ARRAY_WIDTH * ARRAY_HEIGHT)

// Sweep timing. One full cycle: ramp up, hold at full, ramp down, hold dark.
#define RAMP_MS        12000
#define HOLD_MS         3000

// Photoresistor. GP15 is driven HIGH as its 3.3V supply; GP26 (analog channel
// 0) reads the voltage across the chip's internal pull-down, which stands in
// for the fixed resistor of a normal divider. Brighter light = higher reading.
#define LDR_POWER_PIN  15
#define LDR_SENSE_PIN  26

CRGB leds[NUM_LEDS];

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

// Brightness for the current point in the sweep, 0..255.
//
// The ramp is squared rather than linear. Eyes are far more sensitive to
// changes at the dim end, so a linear ramp would seem to jump out of darkness
// and then spend most of its time looking "about full". Squaring spends more
// of the ramp at low levels, which is where the night setting will be.
static uint8_t sweepBrightness(uint32_t now) {
  const uint32_t cycle = 2 * RAMP_MS + 2 * HOLD_MS;
  uint32_t t = now % cycle;

  uint32_t x;   // linear position 0..255
  if (t < RAMP_MS) {
    x = t * 255 / RAMP_MS;                               // ramping up
  } else if (t < RAMP_MS + HOLD_MS) {
    x = 255;                                             // holding at full
  } else if (t < 2 * RAMP_MS + HOLD_MS) {
    x = 255 - (t - RAMP_MS - HOLD_MS) * 255 / RAMP_MS;   // ramping down
  } else {
    x = 0;                                               // holding dark
  }
  return (x * x) / 255;
}

void setup() {
  Serial.begin(115200);
  setupLightSensor();

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  // Show each brightness level as the real hardware level. With dithering on,
  // FastLED flickers between neighbouring levels to fake in-between values,
  // which shimmers visibly at the dim end and would make the low levels hard
  // to judge.
  FastLED.setDither(DISABLE_DITHER);

  fill_solid(leds, NUM_LEDS, CRGB::Red);
}

void loop() {
  uint8_t brightness = sweepBrightness(millis());
  FastLED.setBrightness(brightness);
  FastLED.show();
  delay(20);

  EVERY_N_MILLISECONDS(500) {
    Serial.printf("brightness %3u/255 | light %4u/4095\n", brightness, readLight());
  }
}
