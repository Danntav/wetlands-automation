#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "DataStructures.h"


// menu
enum MenuState {
  MENU_MAIN,
  MENU_BOARD_STATUS,
  MENU_ERROR_LIST,
  MENU_CLEAR_ERRORS
};

extern MenuState currentState;
extern int selectedOption;
extern int currentBoardIndex;
extern struct_message boards[9];

// Initialize display
void initMenu(Adafruit_ST7735 *display);

// Update state based on joystick input
void updateMenu(bool left, bool right, bool up, bool down, bool click);

// Update display
void drawMenu();

void drawBoardStatus();
void drawErrorList();
void clearErrors();

#endif