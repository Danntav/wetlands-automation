#ifndef _MENU_MANAGER_H_
#define _MENU_MANAGER_H_

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "DataStructures.h"


// menu
enum MenuState {
  MENU_MAIN,
  MENU_BOARD_STATUS,
  MENU_ERROR_LIST,
  MENU_ALARM_LIST
};

extern MenuState currentState;
extern int selectedOption;
extern int currentBoardIndex;
extern struct_message boards[TOTAL_SLAVES];
extern String errorMsgs[MAX_ERRORS];
extern int errorCount;

// Initialize display
void initMenu(Adafruit_ST7735 *display);

// Update state based on joystick input
void updateMenu(bool left, bool right, bool up, bool down, bool click);

// Update display
void drawMenu();

void drawBoardStatus();
void drawErrorList();
void clearErrors();
void animateTitle();
bool beginDisplayOperation();
void endDisplayOperation();
String getTimeElapsed(int boardIndex);
void drawAlarmList();

#endif