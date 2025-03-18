#include <FastLED.h>
// Define LED strip parameters
#define LED_PIN 5
#define NUM_LEDS 60
#define BRIGHTNESS 50
#define LED_TYPE WS2812
#define COLOR_ORDER GRB
// Create LED array
CRGB leds[NUM_LEDS];
// Transition time in milliseconds
#define TRANSITION_TIME 2000
// Pause time at extremes in milliseconds
#define PAUSE_TIME 2000
// Startup indicator duration in milliseconds
#define STARTUP_DURATION 1000

// Track the current state
unsigned long lastChangeTime = 0;
enum State {WARM_PAUSE, TRANSITIONING_TO_COLD, COLD_PAUSE, TRANSITIONING_TO_WARM};
State currentState = WARM_PAUSE;
float transitionProgress = 0.0;

void setup() {
  Serial.begin(115200);
  Serial.println("Connected to serial");
  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  // Set all LEDs to red to indicate the sketch has been uploaded
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
  
  // Wait for a moment to display the red color
  delay(STARTUP_DURATION);
  
  // Clear the LEDs after the startup indicator
  FastLED.clear();
  FastLED.show();
  
  // Initialize timing
  lastChangeTime = millis();
}

void loop() {
  // Define warm white (much more amber/orange) and cold white (more blue-tinted)
  CRGB warmWhite = CRGB(255, 180, 107);  // Warmer white (around 2700K)
  CRGB coldWhite = CRGB(220, 235, 255);  // Colder white (around 7500K)
  
  // Get current time
  unsigned long currentMillis = millis();
  unsigned long elapsedTime = currentMillis - lastChangeTime;
  
  // State machine to handle the different phases
  switch(currentState) {
    case WARM_PAUSE:
      if (elapsedTime >= PAUSE_TIME) {
        currentState = TRANSITIONING_TO_COLD;
        lastChangeTime = currentMillis;
      }
      fill_solid(leds, NUM_LEDS, warmWhite);
      break;
      
    case TRANSITIONING_TO_COLD:
      if (elapsedTime >= TRANSITION_TIME) {
        currentState = COLD_PAUSE;
        lastChangeTime = currentMillis;
      } else {
        // Use sine wave for smooth transition
        transitionProgress = (float)elapsedTime / TRANSITION_TIME;
        float sinValue = sin(transitionProgress * PI / 2.0); // Quarter sine wave from 0 to π/2
        CRGB currentColor = blend(warmWhite, coldWhite, 255 * sinValue);
        fill_solid(leds, NUM_LEDS, currentColor);
      }
      break;
      
    case COLD_PAUSE:
      if (elapsedTime >= PAUSE_TIME) {
        currentState = TRANSITIONING_TO_WARM;
        lastChangeTime = currentMillis;
      }
      fill_solid(leds, NUM_LEDS, coldWhite);
      break;
      
    case TRANSITIONING_TO_WARM:
      if (elapsedTime >= TRANSITION_TIME) {
        currentState = WARM_PAUSE;
        lastChangeTime = currentMillis;
      } else {
        // Use sine wave for smooth transition
        transitionProgress = (float)elapsedTime / TRANSITION_TIME;
        float sinValue = sin((transitionProgress * PI / 2.0) + (PI / 2.0)); // Quarter sine wave from π/2 to π
        CRGB currentColor = blend(coldWhite, warmWhite, 255 * (1.0 - sinValue));
        fill_solid(leds, NUM_LEDS, currentColor);
      }
      break;
  }
  
  // Show the LEDs
  FastLED.show();
  
  // Small delay to control how often we update
  delay(20);
}