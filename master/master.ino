#include <esp_now.h>
#include <WiFi.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "LedAnimations.h"
#include "MenuManager.h"
#include "DataStructures.h"


#define BOARD_STATUS_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 min
#define TOTAL_SENDERS 9
#define NODE_ID_UNKNOWN 0
#define MAX_ERRORS 10

// Display pins
#define TFT_CS  15
#define TFT_RST 4
#define TFT_DC  2
#define TFT_LED  13   // pino do backlight
// Other TFT Pins follows the ESP32 VSPI Pattern, no need to specify them
// SDA 23
// SCK 18

// Joystick pins
#define JOY_X_PIN 32
#define JOY_Y_PIN 33
#define JOY_BTN_PIN 25

// SD card
#define SD_CS_PIN  5
#define SD_FILE    "/data.csv"
// Other SD Pins follows the ESP32 VSPI Pattern, no need to specify them
// SD_MOSI 23
// SD_MISO 19
// SD_SCK 18

File sdFile;

// Map all sender's MAC
const uint8_t senderMacs[TOTAL_SENDERS][6] = {
  {0x00,0x4B,0x12,0x21,0x32,0xA8},  // Sender 1
  {0xE0,0xE2,0xE6,0x63,0x15,0xA0},  // Sender 2
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 3
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 4
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 5
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 6
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 7
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 8
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 9
};

// Master's MAC
//uint8_t masterMacAddress[] = {0x14, 0x33, 0x5C, 0x02, 0xED, 0x6C};

// Structure to send ack. Must match the sender structure
typedef struct ack_message {
  int id;
  bool ok;
} ack_message;

// ---------- Joystick struct ----------
struct JoystickState {
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool click = false;
};

JoystickState readJoystick() {
  const int deadZone = 300;  // margem para ignorar ruído no centro
  const int center = 2048;   // valor médio do ADC para 3.3V

  int x = analogRead(JOY_X_PIN);
  int y = analogRead(JOY_Y_PIN);
  bool btn = digitalRead(JOY_BTN_PIN) == LOW;

  JoystickState js;

  if (x < center - deadZone) js.left = true;
  if (x > center + deadZone) js.right = true;
  if (y < center - deadZone) js.up = true;
  if (y > center + deadZone) js.down = true;
  js.click = btn;

  return js;
}

JoystickState prevJs;              // armazena a última leitura
unsigned long lastNavTime = 0;     // timestamp da última ação de navegação
const unsigned long NAV_DEBOUNCE = 300;  // 300 ms entre mudanças

unsigned long lastJoyMove = 0;
// -------------------------------------

String errorLog[MAX_ERRORS];
int errorCount = 0;

MenuState currentState = MENU_MAIN;
int selectedOption = 0;
int currentBoardIndex = 0;
struct_message boards[TOTAL_SENDERS];

// Init ST7735 display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


void setup() {
  Serial.begin(115200);
  ledTurnOff();

  // Configure ESP-NOW  **  SD Card  **  Display  **  Joystick
  setupESPNow();
  setupSD();
  setupDisplay();
  setupJoy();
 
}
 
void loop() {
  JoystickState js = readJoystick();
    unsigned long now = millis();
    if (now - lastNavTime > NAV_DEBOUNCE) {
    // esquerda/direita pra trocar board, cima/baixo no menu
    if (js.left && !prevJs.left) {
      updateMenu(true, false, false, false, false);
      lastNavTime = now;
    } 
    else if (js.right && !prevJs.right) {
      updateMenu(false, true, false, false, false);
      lastNavTime = now;
    }
    else if (js.up && !prevJs.up) {
      updateMenu(false, false, true, false, false);
      lastNavTime = now;
    }
    else if (js.down && !prevJs.down) {
      updateMenu(false, false, false, true, false);
      lastNavTime = now;
    }
    // botão de select (click); lembre-se: INPUT_PULLUP → LOW quando pressionado
    else if (js.click && !prevJs.click) {
      updateMenu(false, false, false, false, true);
      lastNavTime = now;
    }
  }
  prevJs = js;

  ledUpdateNewData();
  ledUpdateBlinking();
  boardStatus();
}


void setupESPNow() {
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR init ESP-NOW");
    logError("ERROR init ESP-NOW");
    ledAnimationBlinking(LED::red, 20);
    return;
  }

  // Register for Recv CB to get the status of packets  
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("Successfully initialized ESP-NOW");
  ledAnimationBlinking(LED::green, 5);
}


void setupSD(){
  // Init SD module
  if (!initSD()) {
    Serial.println("ERROR init SD at setup!");
    logError("ERROR init SD at setup!");
    ledAnimationBlinking(LED::red, 20);
  }
}


void setupDisplay(){
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  initMenu(&tft);
}


void setupJoy() {
  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);
}


void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // Payload extract
  ledNewData();
  if (len != sizeof(struct_message)) return;  // sanity check
  struct_message msg;
  memcpy(&msg, data, sizeof(msg));

  // Identifies sender by ID
  int idx = msg.id - 1;  
  if (idx < 0 || idx >= TOTAL_SENDERS) return;
  boards[idx] = msg;

  // Save msg on SD
  logToSD(msg);

  // Debug
  int senderId = findSenderIndex(info->src_addr);
  char macStr[18];
  if (senderId >= 0) {
    snprintf(macStr, sizeof(macStr), "Sender %d", senderId + 1);
  } else {
    strcpy(macStr, "Unknown");
  }

  Serial.printf("Received from %s (MAC %02X:%02X:%02X:%02X:%02X:%02X) "
                "→ ID=%d, V=%.2fV, T=%.2f°C\n",
    macStr,
    info->src_addr[0],info->src_addr[1],info->src_addr[2],
    info->src_addr[3],info->src_addr[4],info->src_addr[5],
    msg.id, msg.voltage, msg.temperature
  );

  // Send ACK back
  ledNewData();
  ack_message ack = { .id = msg.id, .ok = true };
  esp_now_send(info->src_addr, (uint8_t *)&ack, sizeof(ack));
}


int findSenderIndex(const uint8_t *mac) {
  for (int i = 0; i < TOTAL_SENDERS; i++) {
    if (memcmp(mac, senderMacs[i], 6) == 0) return i;
  }
  return -1;
}


void boardStatus(){
  static unsigned long last = 0;

  if (millis() - last > BOARD_STATUS_INTERVAL_MS) {
    last = millis();
    Serial.println("--- Board Status ---");
    for (int i = 0; i < TOTAL_SENDERS; i++) {
      if (boards[i].id != NODE_ID_UNKNOWN) {
        Serial.printf("Board %d → V=%.2fV, T=%.2f°C\n",
          boards[i].id,
          boards[i].voltage,
          boards[i].temperature
        );
      }
    }

    Serial.println("-------------------------");
  }
}


bool initSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("ERROR: SD CS!");
    logError("ERROR: SD CS!");
    ledAnimationBlinking(LED::red, 20);
    return false;
  }

  Serial.println("SD successfully initialized.");
  ledAnimationBlinking(LED::green, 5);

  // If file doesnt exist, initialize with this header
  if (!SD.exists(SD_FILE)) {
    File f = SD.open(SD_FILE, FILE_WRITE);
    if (f) {
      f.println("timestamp,id,voltage,temperature");
      f.close();
    }
  }
  return true;
}


void logToSD(const struct_message &msg) {
  sdFile = SD.open(SD_FILE, FILE_APPEND);
  if (!sdFile) {
    Serial.println("ERROR: file didn't open!");
    logError("ERROR: file didn't open!");
    ledAnimationBlinking(LED::red, 20);
    return;
  }
  // @@NEED TO ADD RTC 
  unsigned long ts = millis();
  sdFile.printf("%lu,%d,%.2f,%.2f\n", ts, msg.id, msg.voltage, msg.temperature);
  sdFile.close();
}


// String getTimestamp() {
//   // Placeholder com millis() para testes
//   unsigned long seconds = millis() / 1000;
//   int minutes = (seconds / 60) % 60;
//   int hours = (seconds / 3600) % 24;
//   int secs = seconds % 60;

//   char buf[20];
//   snprintf(buf, sizeof(buf), "2025-01-01 %02d:%02d:%02d", hours, minutes, secs);
//   return String(buf);
// }


void logError(String msg) {
  if (errorCount < MAX_ERRORS) {
    errorLog[errorCount++] = msg;
  } else {
    // 
    for (int i = 1; i < MAX_ERRORS; i++) {
      errorLog[i - 1] = errorLog[i];
    }
    errorLog[MAX_ERRORS - 1] = msg;
  }
}