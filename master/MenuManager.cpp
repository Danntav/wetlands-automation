#include "MenuManager.h"
#include "DataStructures.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

extern struct_message boards[TOTAL_SLAVES];
extern String getTimestamp();
extern String errorMsgs[MAX_ERRORS];
extern int errorCount;
extern String boardLastUpdate[TOTAL_SLAVES];
extern unsigned long boardLastUpdateMillis[TOTAL_SLAVES];
extern String alarmMsgs[MAX_ALARMS];
extern int alarmCount;
extern bool hasActiveAlarms;
extern int alarmMenuOption;
extern void clearAlarms();

static Adafruit_ST7735* tft;

unsigned long lastAnimTime = 0;
int titleAnimIndex = 0;
static bool lastFrameCleared = false;
const char* title = "YARA WETLANDS";

// Controla a opção selecionada na tela de errors
int errorMenuOption = 0; // 0 = Voltar, 1 = Clear Errors

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
        selectedOption = (selectedOption + 2) % 3;
        drawMenu();
      } else if (down) {
        selectedOption = (selectedOption + 1) % 3;
        drawMenu();
      }

      if (click) {
        if (selectedOption == 0) {
          currentState = MENU_BOARD_STATUS;
          currentBoardIndex = 0;
          drawBoardStatus();
        } else if (selectedOption == 1) {
          currentState = MENU_ERROR_LIST;
          errorMenuOption = 0; // Reset para "Voltar"
          drawErrorList();
        } else if (selectedOption == 2) {
          currentState = MENU_ALARM_LIST;
          alarmMenuOption = 0;
          drawAlarmList();
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
      if (left) {
        errorMenuOption = (errorMenuOption + 1) % 2; // Alterna entre Voltar e Clear
        drawErrorList();
      } else if (right) {
        errorMenuOption = (errorMenuOption + 1) % 2; // Alterna entre Voltar e Clear
        drawErrorList();
      } else if (click) {
        if (errorMenuOption == 0) {
          // Voltar
          currentState = MENU_MAIN;
          titleAnimIndex = 0;
          lastFrameCleared = false;
          tft->fillScreen(ST77XX_BLACK);
          drawMenu();
        } else {
          clearErrors();
          drawErrorList();
          Serial.println("Cleared Errors");
        }
      }
      break;

    case MENU_ALARM_LIST:
      if (left) {
        alarmMenuOption = (alarmMenuOption + 1) % 2;
        drawAlarmList();
      } else if (right) {
        alarmMenuOption = (alarmMenuOption + 1) % 2;
        drawAlarmList();
      } else if (click) {
        if (alarmMenuOption == 0) {
          // Voltar
          currentState = MENU_MAIN;
          titleAnimIndex = 0;
          lastFrameCleared = false;
          tft->fillScreen(ST77XX_BLACK);
          drawMenu();
        } else {
          // Clear Alarms
          clearAlarms();
          drawAlarmList();
        }
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

  const char* options[] = { "Reatores", "Mestre Confg", "Alarmes" };
  const int count = sizeof(options) / sizeof(options[0]);

  for (int i = 0; i < count; i++) {
    tft->fillRect(5, 60 + i * 20, 150, 18, i == selectedOption ? ST77XX_WHITE : ST77XX_BLACK);

    if (i == selectedOption) {
      tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);
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
    tft->printf("REATOR %d NOK", currentBoardIndex + 1);
  } else {
    tft->printf("REATOR %d", board.id);
  }
  //Timestamp from last packet
  tft->setTextSize(1);
  tft->setCursor(10, 45);
  tft->setTextColor(ST77XX_CYAN);
  tft->print("Ultima coleta:");
  
  tft->setCursor(10, 55);
  if (boardLastUpdateMillis[currentBoardIndex] == 0) {
    tft->setTextColor(ST77XX_RED);
    tft->print("Nenhum dado recebido");
  } else {
    tft->setTextColor(ST77XX_WHITE);
    // Mostra apenas a parte do tempo (HH:MM:SS) para economizar espaço
    String fullTimestamp = boardLastUpdate[currentBoardIndex];
    String timeOnly = fullTimestamp.substring(11); // Pega só "HH:MM:SS"
    tft->print(timeOnly);
    
    // Tempo decorrido
    tft->setTextColor(ST77XX_YELLOW);
    tft->print("(");
    tft->print(getTimeElapsed(currentBoardIndex));
    tft->print(")");
  }

  tft->setTextSize(1);
  tft->setCursor(10, 35);
  tft->setTextColor(ST77XX_CYAN);
  tft->print("TS: ");
  tft->print(getTimestamp());

  tft->setTextSize(2);
  tft->setTextColor(ST77XX_GREEN);
  tft->setCursor(10, 65);
  if (board.id == 0) {
    tft->print("V: --- V");
  } else {
    String voltageString = String(board.voltage, 4); // Converte o float para String com 4 casas decimais
    if (voltageString.startsWith("0.")) {
      voltageString = voltageString.substring(1); // Remove o "0" inicial, mantendo o "."
    }
    tft->print("V: ");
    tft->print(voltageString);
    tft->print("mV");
  }

  tft->setCursor(10, 85);
  if (board.id == 0) {
    tft->print("T: --- C");
  } else {
    tft->printf("T: %.2f C", board.temperature);
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
  
  if (errorCount == 0) {
    tft->setCursor(10, y);
    tft->print("Nenhum erro registrado");
  } else {
    for (int i = 0; i < errorCount && y < 95; i++) {
      tft->setCursor(10, y);
      tft->print(errorMsgs[i]);
      y += 18;
    }
  }
  
  // Desenha as opções na parte inferior
  tft->setTextSize(1);
  
  // Opção "Voltar"
  if (errorMenuOption == 0) {
    tft->fillRect(5, 105, 70, 12, ST77XX_WHITE);
    tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);
  } else {
    tft->setTextColor(ST77XX_WHITE);
  }
  tft->setCursor(10, 107);
  tft->print("< Voltar");
  
  // Opção "Clear Errors"
  if (errorMenuOption == 1) {
    tft->fillRect(85, 105, 70, 12, ST77XX_WHITE);
    tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);
  } else {
    tft->setTextColor(ST77XX_WHITE);
  }
  tft->setCursor(90, 107);
  tft->print("Limpar >");  
  endDisplayOperation();
}

void clearErrors() {
  for (int i = 0; i < errorCount; i++) {
    errorMsgs[i] = "";
  }
  errorCount = 0;
}

void animateTitle() {
  if (currentState != MENU_MAIN) return;
  if (!beginDisplayOperation()) return;
  unsigned long now = millis();
  if (now - lastAnimTime > 300) {
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


String getTimeElapsed(int boardIndex) {
  if (boardLastUpdateMillis[boardIndex] == 0) {
    return "Never";
  }
  
  unsigned long elapsed = millis() - boardLastUpdateMillis[boardIndex];
  unsigned long seconds = elapsed / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  if (hours > 0) {
    return String(hours) + "h " + String(minutes % 60) + "m";
  } else if (minutes > 0) {
    return String(minutes) + "m " + String(seconds % 60) + "s";
  } else {
    return String(seconds) + "s atras";
  }
}



void drawAlarmList() {
  if (!beginDisplayOperation()) return;
  
  tft->fillScreen(ST77XX_BLACK);
  tft->setTextSize(2);
  tft->setTextColor(ST77XX_RED);
  tft->setCursor(10, 10);
  tft->print("ALARMES:");

  tft->setTextSize(1);
  tft->setTextColor(ST77XX_WHITE);
  
  // Coleta status de todos os slaves
  String activeAlarms = "";
  for (int i = 1; i <= TOTAL_SLAVES; i++) {
    String status = getSlaveStatus(i);
    if (status.length() > 0) {
      if (activeAlarms.length() > 0) activeAlarms += " ";
      activeAlarms += status;
    }
  }
  
  if (activeAlarms.length() == 0) {
    tft->setCursor(10, 35);
    tft->setTextColor(ST77XX_GREEN);
    tft->print("Sistema OK");
  } else {
    tft->setCursor(10, 35);
    tft->setTextColor(ST77XX_YELLOW);
    tft->print("Problemas:");
    
    tft->setCursor(10, 50);
    tft->setTextColor(ST77XX_RED);
    tft->print(activeAlarms);
    
    // Legenda
    tft->setCursor(2, 82);
    tft->setTextColor(ST77XX_CYAN);
    tft->setTextSize(1);
    tft->print("X=Timeout V=Voltage T=Temp");
  }
  
  tft->setTextSize(1);
  
  if (alarmMenuOption == 0) {
    tft->fillRect(5, 105, 70, 12, ST77XX_WHITE);
    tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);
  } else {
    tft->setTextColor(ST77XX_WHITE);
  }
  tft->setCursor(10, 107);
  tft->print("< Voltar");
  
  if (alarmMenuOption == 1) {
    tft->fillRect(85, 105, 70, 12, ST77XX_WHITE);
    tft->setTextColor(ST77XX_BLACK, ST77XX_WHITE);
  } else {
    tft->setTextColor(ST77XX_WHITE);
  }
  tft->setCursor(90, 107);
  tft->print("Limpar >");
  
  endDisplayOperation();
}