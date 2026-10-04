// DomeLight - 2x8 WS2812B array on a Waveshare RP2040-Zero.
//
// Lights every LED red. The dimness comes ONLY from the current cap below:
// brightness stays at 255, and FastLED's power limiter turns it down at each
// show() to keep its estimated draw under MAX_MILLIAMPS.
//
// Wiring:
//   Array DIN -> GP6 (pad 17, right edge), ideally through a 330-470 ohm
//                resistor. GP6 is 3.3V logic; see the README if the first
//                pixel flickers or shows the wrong colour.
//   Array GND -> board GND (always, whatever powers the array)
//   Array 5V  -> board 5V pad while on USB; a buck converter once mounted.
//                Read the README's power section before switching over.

#include <Arduino.h>
#include <FastLED.h>

#define DATA_PIN       6
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

#define ARRAY_WIDTH    8
#define ARRAY_HEIGHT   2
#define NUM_LEDS       (ARRAY_WIDTH * ARRAY_HEIGHT)

// Current budget for the limiter, in mA at 5V.
//
// FastLED's default model counts a full-red pixel as 16mA, an unlit pixel as
// 1mA, and the MCU as a flat 25mA. All 16 pixels at full red is ~296mA by that
// model, so a 25mA cap scales brightness to about 255 * 25 / 296 = 21.
//
// The cap is an estimate, not a measured limit. The formula scales the
// unlit-pixel and MCU terms along with brightness even though real hardware
// keeps drawing them, so expect the actual current to be a bit above 25mA -
// roughly 35-40mA for the array plus whatever the board itself uses.
#define MAX_MILLIAMPS  25

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);

  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
}

void loop() {
  // Re-sending the frame is cheap, and it means the LEDs recover on their own
  // if the array is powered up after the board.
  FastLED.show();
  delay(20);

  // Print the brightness the limiter actually chose, so the cap can be checked
  // over USB serial (`make monitor`).
  EVERY_N_SECONDS(2) {
    uint8_t effective = calculate_max_brightness_for_power_mW(255, 5 * MAX_MILLIAMPS);
    Serial.printf("cap %d mA -> effective brightness %u/255\n", MAX_MILLIAMPS, effective);
  }
}
