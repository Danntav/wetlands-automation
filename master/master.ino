#include <esp_now.h>
#include <WiFi.h>
#include <RTClib.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Wire.h>
#include "LedAnimations.h"
#include "MenuManager.h"
#include "DataStructures.h"

#define BOARD_STATUS_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 min
#define NODE_ID_UNKNOWN 0

// Map all slave's MAC
const uint8_t slaveMacs[TOTAL_SLAVES][6] = {
  {0x00,0x4B,0x12,0x21,0x32,0xA8},  // Slave 1
  {0xE0,0xE2,0xE6,0x63,0x15,0xA0},  // Slave 2
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Slave 3
  {0x14,0x33,0x5C,0x02,0xED,0xA1},  // Slave 4
  {0x14,0x33,0x5C,0x02,0xED,0xA2},  // Slave 5
  {0x14,0x33,0x5C,0x02,0xED,0xA3},  // Slave 6
  {0x14,0x33,0x5C,0x02,0xED,0xA4},  // Slave 7
  {0x14,0x33,0x5C,0x02,0xED,0xA5},  // Slave 8
  {0x14,0x33,0x5C,0x02,0xED,0xA6},  // Slave 9
 };

// Master's MAC
//uint8_t masterMacAddress[] = {0x14, 0x33, 0x5C, 0x02, 0xED, 0x6C};


JoystickState readJoystick() {
  const int deadZone = 600;  // margem para ignorar ruído no centro
  const int center = 2048;
  static int prevX = center, prevY = center; // Armazena leituras anteriores para média móvel
  const float alpha = 0.7;   // valor médio do ADC para 3.3V

  int rawX = analogRead(JOY_X_PIN);
  int rawY = analogRead(JOY_Y_PIN);
  bool btn = digitalRead(JOY_BTN_PIN) == LOW;

  // Aplica média móvel para suavizar ruído
  int x = alpha * rawX + (1 - alpha) * prevX;
  int y = alpha * rawY + (1 - alpha) * prevY;
  prevX = x;
  prevY = y;

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
const unsigned long NAV_DEBOUNCE = 200;  // 150 ms entre mudanças
unsigned long lastJoyMove = 0;
// -------------------------------------

String errorMsgs[MAX_ERRORS];
int errorCount = 0;

MenuState currentState = MENU_MAIN;
int selectedOption = 0;
int currentBoardIndex = 0;
struct_message boards[TOTAL_SLAVES];


RTC_DS1307 rtc; //DS1307

// Init ST7735 display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

bool sdInitialized = false;


void setup() {
  Serial.begin(115200);
  delay(1500);
  ledTurnOff();
  setupLeds();
  setupDisplay();
  delay(1500);
  setupRTC();
  delay(1000);

  for (int i = 0; i < TOTAL_SLAVES; i++) {
    boards[i].id = NODE_ID_UNKNOWN;
    boards[i].voltage = 0.0;
    boards[i].temperature = 0.0;
  }
  setupESPNow();
  delay(1000);
  setupJoy();
  
}
 
void loop() {
  handleJoystick();
  ledUpdateNewData();
  ledUpdateBlinking();
  boardStatus();
  animateTitle();
}


void setupESPNow() {
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR init ESP-NOW");
    logErrorDisplay("ERROR init ESP-NOW");
    ledAnimationBlinking(LED::red, 20);
    return;
  }
  esp_now_register_recv_cb(onDataRecv);
  // Register all Slaves
  for (int i = 0; i < TOTAL_SLAVES; i++) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, slaveMacs[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.printf("Failed to add peer %d\n", i + 1);
      logErrorDisplay("Failed to add peer");
      ledAnimationBlinking(LED::red, 20);
    }
  }
  delay(1000);
  Serial.println("ESP-NOW Successfully initialized");
  ledAnimationBlinking(LED::green, 5);
}


void setupRTC(){
  Wire.begin(RTC_SDA, RTC_SCL);
  scanI2C();
  if (!rtc.begin()) {
    Serial.println("ERROR RTC not found!");
    logErrorDisplay("ERROR RTC not found!");
    ledAnimationBlinking(LED::red, 20);
  } else if (!rtc.isrunning()) {
    Serial.println("ERROR RTC not running");
    logErrorDisplay("ERROR RTC not running");
    ledAnimationBlinking(LED::red, 20);
    // Ajustar data/hora inicial (ajuste para a data atual, ex.: 17/07/2025 20:21:00)
    rtc.adjust(DateTime(2025, 7, 19, 16, 30, 0));
  } else {
    Serial.println("RTC successfully initialized");
    ledAnimationBlinking(LED::green, 5);
  }
}


void setupDisplay() {
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  delay(500);
  digitalWrite(TFT_CS, LOW);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);
  Serial.println("Initializing display...");
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  Serial.println("Display initialized successfully");
  digitalWrite(TFT_CS, HIGH);
  SPI.end();
  Serial.println("Starting initMenu...");
  initMenu(&tft);
  Serial.println("setupDisplay completed");
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

  // Identifies slave by ID
  int idx = msg.id - 1;  
  if (idx < 0 || idx >= TOTAL_SLAVES) return;
  boards[idx] = msg;

  // Save msg on SD
  logToSD(msg);

  // Debug
  int slaveId = findSlaveIndex(info->src_addr);
  char macStr[18];
  if (slaveId >= 0) {
    snprintf(macStr, sizeof(macStr), "Slave %d", slaveId + 1);
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
  esp_err_t result = esp_now_send(info->src_addr, (uint8_t *)&ack, sizeof(ack));
  if (result != ESP_OK) {
    char errorMsgACK[32];
    snprintf(errorMsgACK, sizeof(errorMsgACK), "ERROR send ACK S%d", msg.id);
    Serial.printf(errorMsgACK);
    logErrorDisplay(errorMsgACK);
    ledAnimationBlinking(LED::red, 20);
  }
}


int findSlaveIndex(const uint8_t *mac) {
  for (int i = 0; i < TOTAL_SLAVES; i++) {
    if (memcmp(mac, slaveMacs[i], 6) == 0) return i;
  }
  return -1;
}


void boardStatus(){
  static unsigned long last = 0;

  if (millis() - last > BOARD_STATUS_INTERVAL_MS) {
    last = millis();
    Serial.println("--- Board Status ---");
    for (int i = 0; i < TOTAL_SLAVES; i++) {
      if (boards[i].id != NODE_ID_UNKNOWN) {
        Serial.printf("Board %d → V=%.3fV, T=%.3f°C\n",
          boards[i].id,
          boards[i].voltage,
          boards[i].temperature
        );
      }
    }

    Serial.println("-------------------------");
  }
}


void logToSD(const struct_message &msg) {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(200);
  digitalWrite(SD_CS, LOW);
  Serial.println("Initializing SD...");
  sdInitialized = SD.begin(SD_CS);
  if (!sdInitialized) {
    Serial.println("ERROR init SD!");
    digitalWrite(TFT_CS, LOW);
    logErrorDisplay("ERROR init SD!");
    digitalWrite(TFT_CS, HIGH);
    ledAnimationBlinking(LED::red, 20);
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  Serial.println("SD initialized successfully");
  File sdFile = SD.open(SD_FILE, FILE_APPEND);
  if (!sdFile) {
    Serial.println("ERROR: file didn't open!");
    digitalWrite(TFT_CS, LOW);
    logErrorDisplay("ERROR: file didn't open!");
    digitalWrite(TFT_CS, HIGH);
    ledAnimationBlinking(LED::red, 20);
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  String timestamp = getTimestamp();
  sdFile.printf("%s,%d,%.2f,%.2f\n", timestamp.c_str(), msg.id, msg.voltage, msg.temperature);
  sdFile.close();
  Serial.println("Data written to SD");
  digitalWrite(SD_CS, HIGH);
  SPI.end();
}


String getTimestamp() {
  if (!rtc.isrunning()) {
    Serial.println("ERROR RTC not running");
    logErrorDisplay("ERROR RTC not running");
    return "1970-01-01 00:00:00"; 
  }
  DateTime now = rtc.now();
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buf);
}


void scanI2C() {
  Serial.println("Scanning I2C bus...");
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Device found at address 0x%02X\n", addr);
    }
  }
}


void logErrorDisplay(String msg) {
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);
  delay(10);
  digitalWrite(TFT_CS, LOW);
  String timestamp = getTimestamp();
  String fullErrorMsg = timestamp + "@" + msg;
  if (errorCount < MAX_ERRORS) {
    errorMsgs[errorCount++] = fullErrorMsg;
  } else {
    for (int i = 1; i < MAX_ERRORS; i++) {
      errorMsgs[i - 1] = errorMsgs[i];
    }
    errorMsgs[MAX_ERRORS - 1] = fullErrorMsg;
  }
  digitalWrite(TFT_CS, HIGH);
  SPI.end();
}


void handleJoystick() {
  JoystickState js = readJoystick();
  unsigned long now = millis();
  if (now - lastNavTime > NAV_DEBOUNCE) {
    if (js.left && !prevJs.left) {
      updateMenu(true, false, false, false, false);
      lastNavTime = now;
    } else if (js.right && !prevJs.right) {
      updateMenu(false, true, false, false, false);
      lastNavTime = now;
    } else if (js.up && !prevJs.up) {
      updateMenu(false, false, true, false, false);
      lastNavTime = now;
    } else if (js.down && !prevJs.down) {
      updateMenu(false, false, false, true, false);
      lastNavTime = now;
    } else if (js.click && !prevJs.click) {
      updateMenu(false, false, false, false, true);
      lastNavTime = now;
    }
  }
  prevJs = js;
}