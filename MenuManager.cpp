#include "MenuManager.h"
#include "DataStructures.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

extern struct_message boards[9];
//extern String getCurrentTimestamp(); @@ FALTA IMPLEMENTAR RTC

extern String errorLog[];
extern int errorCount;

static Adafruit_ST7735* tft;

void initMenu(Adafruit_ST7735* display) {
  tft = display;
  currentState = MENU_MAIN;
  selectedOption = 0;
  drawMenu();
}

void updateMenu(bool left, bool right, bool up, bool down, bool click) {
  switch (currentState) {
    case MENU_MAIN:
      if (up) {
        selectedOption = (selectedOption + 2) % 3;  // navega para cima
        drawMenu();
      } else if (down) {
        selectedOption = (selectedOption + 1) % 3;  // navega para baixo
        drawMenu();
      }

      if (click) {
        if (selectedOption == 0) {
          currentState = MENU_BOARD_STATUS;
          currentBoardIndex = 0;
          drawBoardStatus();
        } else if (selectedOption == 1) {
          currentState = MENU_ERROR_LIST;
          drawErrorList();
        } else if (selectedOption == 2) {
          clearErrors();
          drawMenu();
        }
      }
      break;

    case MENU_BOARD_STATUS:
      if (left) {
        currentBoardIndex = (currentBoardIndex + 8) % 9;
        drawBoardStatus();
      } else if (right) {
        currentBoardIndex = (currentBoardIndex + 1) % 9;
        drawBoardStatus();
      } else if (click) {
        currentState = MENU_MAIN;
        drawMenu();
      }
      break;

    case MENU_ERROR_LIST:
      if (click) {
        currentState = MENU_MAIN;
        drawMenu();
      }
      break;
  }
}

void drawMenu() {
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextSize(2);
  tft->setTextColor(ST77XX_GREEN);
  tft->setCursor(50, 10);
  tft->print("MENU");

  const char* options[] = { "Board Status", "Last Errors", "Clear Errors" };
  const int count = sizeof(options) / sizeof(options[0]);

  for (int i = 0; i < count; i++) {
    if (i == selectedOption) {
      tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);  // destaque invertido
    } else {
      tft->setTextColor(ST77XX_WHITE);
    }
    tft->setCursor(10, 40 + i * 20);
    tft->print(options[i]);
  }
}

void drawBoardStatus() {
  tft->fillScreen(ST77XX_BLACK);
  struct_message& board = boards[currentBoardIndex];

  tft->setTextColor(ST77XX_WHITE);
  tft->setTextSize(2);
  tft->setCursor(40, 10);
  tft->printf("Board %d", board.id);

  tft->setTextSize(1.8);
  tft->setCursor(10, 40);
  tft->setTextColor(ST77XX_CYAN);
  tft->print("TS: ");
  //tft->print(getCurrentTimestamp()); @@

  tft->setTextColor(ST77XX_GREEN);
  tft->setCursor(10, 60);
  tft->printf("V: %.2f V", board.voltage);

  tft->setCursor(10, 80);
  tft->printf("T: %.2f C", board.temperature);



  tft->setTextColor(ST77XX_WHITE);
  tft->setCursor(45, 115);
  tft->print("<  Voltar  >");
}

void drawErrorList() {
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextSize(2);
  tft->setTextColor(ST77XX_RED);
  tft->setCursor(10, 10);
  tft->print("LAST ERRORS:");

  tft->setTextSize(1);
  tft->setTextColor(ST77XX_WHITE);
  int y = 30;
  for (int i = 0; i < errorCount && y < 120; i++) {
    tft->setCursor(10, y);
    tft->print(errorLog[i]);
    y += 12;
  }

  tft->setCursor(10, 115);
  tft->print("< Voltar");
}


void clearErrors() {
  for (int i = 0; i < errorCount; i++) {
    errorLog[i] = "";
  }
  errorCount = 0;
  
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextColor(ST77XX_WHITE);
  tft->setCursor(40, 60);
  tft->print("CLEARED!");
  delay(1000); 
}