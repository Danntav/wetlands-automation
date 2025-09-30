#include <Arduino.h>
#include "LedAnimations.h"

static int blinkPin = -1;
static int blinkTotalBlinks = 3;
static int blinkCount = 0;
static bool blinkState = false;
static unsigned long lastBlinkTime = 0;
static unsigned long blinkDelay = 250;
static bool blinkingActive = false;
static unsigned long ledBlinkUntil = 0;


void setupLeds() {
  const int leds[] = {LED::red, LED::yellow, LED::green};
  const int ledCount = sizeof(leds) / sizeof(leds[0]);
  for (int i = 0; i < ledCount; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }
}


void ledAnimationBlinking(int ledPin, int sec){
  blinkPin = ledPin;
  blinkTotalBlinks = sec * 2;
  blinkCount = 0;
  blinkState = false;
  blinkingActive = true;
  lastBlinkTime = millis();
}


void ledAnimationSnake() {
  static const int leds[] = {LED::red, LED::yellow, LED::green};
  static const int ledCount = sizeof(leds) / sizeof(leds[0]);
  static int currentLed = 0;
  const int delayMS = 100;
  static unsigned long lastUpdate = 0;

  unsigned long now = millis();
  if (now - lastUpdate >= delayMS) {
    for (int i = 0; i < ledCount; i++) {
      digitalWrite(leds[i], LOW);
    }

    digitalWrite(leds[currentLed], HIGH);

    currentLed = (currentLed + 1) % ledCount;
    lastUpdate = now;
  }
}


void ledAnimationPingPong(){
  static const int sequence[][3] = {
    {1, 0, 1}, // red & green
    {0, 1, 0}, // yellow
  };

  static int state = 0;
  static unsigned long lastChange = 0;
  const int delayMS = 250;
  const int leds[] = { LED::red, LED::yellow, LED::green };

  unsigned long now = millis();
  if (now - lastChange >= delayMS) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(leds[i], sequence[state][i]);
    }
    state = (state + 1) % 2;
    lastChange = now;
  }
}


void ledTurnOff(){
  const int leds[] = {LED::red, LED::yellow, LED::green};
  const int ledCount = sizeof(leds) / sizeof(leds[0]);
  for (int i = 0; i < ledCount; i++) {
    digitalWrite(leds[i], LOW);
  }
}


void ledNewData() {
  digitalWrite(LED::yellow, HIGH);
  ledBlinkUntil = millis() + 100;
}


void ledUpdateNewData() {
  if (millis() > ledBlinkUntil) {
    digitalWrite(LED::yellow, LOW);
  }
}


void ledUpdateBlinking() {
  if (!blinkingActive) return;

  unsigned long now = millis();
  if (now - lastBlinkTime >= blinkDelay) {
    blinkState = !blinkState;
    digitalWrite(blinkPin, blinkState ? HIGH : LOW);
    lastBlinkTime = now;

    if (!blinkState) blinkCount++;

    if (blinkCount >= blinkTotalBlinks) {
      blinkingActive = false;
      digitalWrite(blinkPin, LOW);
    }
  }
}