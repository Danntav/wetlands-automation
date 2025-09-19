#ifndef _LED_ANIMATIONS_H_
#define _LED_ANIMATIONS_H_

namespace LED {
  const int yellow = 33;
  const int red = 32;
  const int green = 25;
}

void setupLeds();
void ledAnimationBlinking(int ledPin, int sec);
void ledAnimationSnake();
void ledAnimationPingPong();
void ledTurnOff();
void ledNewData();
void ledUpdateNewData();
void ledUpdateBlinking();

#endif