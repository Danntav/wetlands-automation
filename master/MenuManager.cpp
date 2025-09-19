#include "MenuManager.h"
#include "DataStructures.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

extern struct_message boards[TOTAL_SLAVES];
extern String getTimestamp();
extern String errorMsgs[MAX_ERRORS];
extern int errorCount;

static Adafruit_ST7735* tft;

unsigned long lastAnimTime = 0;
int titleAnimIndex = 0;
static bool lastFrameCleared = false;
const char* title = "YARA WETLANDS";


bool beginDisplayOperation() {
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);
  SPI.setFrequency(1000000);
  delay(10);
  digitalWrite(TFT_CS, LOW);
  if (!tft) {
    Serial.println("ERROR: TFT pointer is null!");
    digitalWrite(TFT_CS, HIGH);
    SPI.end();
    return false;
  }
  return true;
}

void endDisplayOperation() {
  digitalWrite(TFT_CS, HIGH);
  SPI.end();
}


void initMenu(Adafruit_ST7735* display) {
  if (!display) {
    Serial.println("ERROR: TFT pointer is null in initMenu!");
    return;
  }
  tft = display;
  currentState = MENU_MAIN;
  selectedOption = 0;
  Serial.println("Starting initMenu...");
  if (!beginDisplayOperation()) return;
  drawMenu();
  endDisplayOperation();
  Serial.println("initMenu completed");
}

void updateMenu(bool left, bool right, bool up, bool down, bool click) {
  if (currentBoardIndex >= TOTAL_SLAVES || currentBoardIndex < 0) {
    Serial.println("ERROR: Invalid board index!");
    currentBoardIndex = 0;
  }
  if (!beginDisplayOperation()) return;
  switch (currentState) {
    case MENU_MAIN:
      if (up) {
        selectedOption = (selectedOption + 2) % 3;  // navigation up
        drawMenu();
      } else if (down) {
        selectedOption = (selectedOption + 1) % 3;  // navigation down
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
        Serial.printf("Navigating to Board %d\n", currentBoardIndex + 1);
        drawBoardStatus();
      } else if (right) {
        currentBoardIndex = (currentBoardIndex + 1) % 9;
        Serial.printf("Navigating to Board %d\n", currentBoardIndex + 1);
        drawBoardStatus();
      } else if (click) {
        currentState = MENU_MAIN;
        titleAnimIndex = 0;
        lastFrameCleared = false;
        tft->fillScreen(ST77XX_BLACK);
        drawMenu();
      }
      break;

    case MENU_ERROR_LIST:
      if (click) {
        currentState = MENU_MAIN;
        titleAnimIndex = 0; // Reinicia animação
        lastFrameCleared = false;
        tft->fillScreen(ST77XX_BLACK);
        drawMenu();
      }
      break;
  }
  endDisplayOperation();
}

void drawMenu() {
  if (!beginDisplayOperation()) return;
  tft->fillRect(0, 60, 160, 128, ST77XX_BLACK);
  tft->setTextSize(2);
  tft->setTextColor(ST77XX_GREEN);
  tft->setCursor(10, 30);
  tft->print("--- MENU ---");

  const char* options[] = { "Board Status", "Last Errors", "Clear Errors" };
  const int count = sizeof(options) / sizeof(options[0]);

  for (int i = 0; i < count; i++) {
    tft->fillRect(10, 60 + i * 20, 140, 16, i == selectedOption ? ST77XX_WHITE : ST77XX_BLACK);
    if (i == selectedOption) {
      tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);  // inverted highlight
    } else {
      tft->setTextColor(ST77XX_WHITE);
    }
    tft->setCursor(10, 60 + i * 20);
    tft->print(options[i]);
  }
  endDisplayOperation();
}

void drawBoardStatus() {
  if (currentBoardIndex >= TOTAL_SLAVES || currentBoardIndex < 0) {
    Serial.println("ERROR: Invalid board index in drawBoardStatus!");
    currentBoardIndex = 0;
  }
  struct_message& board = boards[currentBoardIndex];
  if (!beginDisplayOperation()) return;
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextColor(ST77XX_WHITE);
  tft->setTextSize(2);
  tft->setCursor(15, 10);
  if (board.id == 0) {
    tft->printf("Board %d NOK", currentBoardIndex + 1);
  } else {
    tft->printf("Board %d", board.id);
  }

  tft->setTextSize(1);
  tft->setCursor(10, 40);
  tft->setTextColor(ST77XX_CYAN);
  tft->print("TS: ");
  tft->print(getTimestamp());

  tft->setTextSize(2);
  tft->setTextColor(ST77XX_GREEN);
  tft->setCursor(10, 60);
  if (board.id == 0) {
    tft->print("V: --- V");
  } else {
    tft->printf("V: %.3f V", board.voltage);
  }

  tft->setCursor(10, 80);
  if (board.id == 0) {
    tft->print("T: --- C");
  } else {
    tft->printf("T: %.3f C", board.temperature);
  }

  tft->setTextSize(1);
  tft->setTextColor(ST77XX_WHITE);
  tft->setCursor(45, 115);
  tft->print("<  Voltar  >");
  endDisplayOperation();
}


void drawErrorList() {
  if (!beginDisplayOperation()) return;
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
    tft->print(errorMsgs[i]);
    y += 18;
  }
  
  tft->setTextColor(ST77XX_WHITE);
  tft->setCursor(10, 115);
  tft->print("< Voltar");
  endDisplayOperation();
}


void clearErrors() {
  for (int i = 0; i < errorCount; i++) {
    errorMsgs[i] = "";
  }
  errorCount = 0;
  if (!beginDisplayOperation()) return;
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextColor(ST77XX_WHITE);
  tft->setCursor(40, 60);
  tft->print("CLEARED!");
  endDisplayOperation();
  delay(1000);
  drawMenu();
}


void animateTitle() {
  if (currentState != MENU_MAIN) return;
  if (!beginDisplayOperation()) return;
  unsigned long now = millis();
  if (now - lastAnimTime > 200) {
    lastAnimTime = now;
    if (!lastFrameCleared) {
      tft->fillRect(0, 0, 160, 30, ST77XX_BLACK);
      lastFrameCleared = true;
    }
    tft->setCursor(titleAnimIndex * 12, 0);
    tft->setTextSize(2);
    tft->setTextColor(ST77XX_ORANGE);
    tft->print(title[titleAnimIndex]);
    titleAnimIndex++;
    if (title[titleAnimIndex] == '\0') {
      titleAnimIndex = 0;
      lastFrameCleared = false;
    }
  }
  endDisplayOperation();
}