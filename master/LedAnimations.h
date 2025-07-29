#ifndef _LED_ANIMATIONS_H_
#define _LED_ANIMATIONS_H_

namespace LED {
  const int yellow = 25;
  const int red = 21;
  const int green = 19;
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