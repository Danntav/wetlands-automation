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

//#define COLLECTION_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 min
#define COLLECTION_INTERVAL_MS (60UL * 1000UL)
#define NODE_ID_UNKNOWN 0

String boardLastUpdate[TOTAL_SLAVES];
unsigned long boardLastUpdateMillis[TOTAL_SLAVES];

String alarmMsgs[MAX_ALARMS];
int alarmCount = 0;
bool hasActiveAlarms = false;
int alarmMenuOption = 0;

// Map all slave's MAC
const uint8_t slaveMacs[TOTAL_SLAVES][6] = {
  { 0x00, 0x4B, 0x12, 0x21, 0x32, 0xA8 },  // Slave 1
  { 0xE0, 0xE2, 0xE6, 0x63, 0x15, 0xA0 },  // Slave 2
  { 0x00, 0x4B, 0x12, 0x22, 0x43, 0xC0 },  // Slave 3
  { 0x38, 0x18, 0x2B, 0xE8, 0x6C, 0x2C },  // Slave 4
  { 0x14, 0x33, 0x5C, 0x02, 0xE8, 0x80 },  // Slave 5
  { 0x00, 0x4B, 0x12, 0x21, 0xE6, 0x68 },  // Slave 6
  { 0x20, 0x43, 0xA8, 0xE6, 0x0B, 0x30 },  // Slave 7
  { 0x14, 0x33, 0x5C, 0x02, 0xED, 0x6C },  // Slave 8
  { 0x14, 0x33, 0x5C, 0x02, 0xED, 0xA6 },  // Slave 9
};

// Master's MAC
//uint8_t masterMacAddress[] = {0x3C, 0x8A, 0x1F, 0x5E, 0x16, 0x48};

JoystickState readJoystick() {
  const int deadZone = 600;  // ignoring center noise
  const int center = 2048;
  static int prevX = center, prevY = center;  // store prev moving average readings
  const float alpha = 0.7;                    // valor médio do ADC para 3.3V

  int rawX = analogRead(JOY_X_PIN);
  int rawY = analogRead(JOY_Y_PIN);
  bool btn = digitalRead(JOY_BTN_PIN) == LOW;

  // Exponential moving average
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

JoystickState prevJs;                    // last reading
unsigned long lastNavTime = 0;           // timestamp of the last move
const unsigned long NAV_DEBOUNCE = 200;  // 200 ms between changes
unsigned long lastJoyMove = 0;
// -------------------------------------

String errorMsgs[MAX_ERRORS];
int errorCount = 0;

MenuState currentState = MENU_MAIN;
int selectedOption = 0;
int currentBoardIndex = 0;
struct_message boards[TOTAL_SLAVES];


RTC_DS1307 rtc;  //DS1307

// Init ST7735 display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

bool sdInitialized = false;


void setup() {
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  
  Serial.begin(115200);
  delay(1500);

  ledTurnOff();
  setupLeds();
  //setupDisplay();
  delay(1500);

  setupRTC();
  delay(1000);

  for (int i = 0; i < TOTAL_SLAVES; i++) {
    boards[i].id = NODE_ID_UNKNOWN;
    boards[i].voltage = 0.0;
    boards[i].temperature = 0.0;
    boardLastUpdate[i] = "Never";
    boardLastUpdateMillis[i] = 0;
  }

  for (int i = 0; i < MAX_ALARMS; i++) {
    alarmMsgs[i] = "";
  }
  alarmCount = 0;
  hasActiveAlarms = false;
  alarmMenuOption = 0;

  setupESPNow();
  delay(1000);
  setupJoy();
}

void loop() {
  //handleJoystick();
  ledUpdateNewData();
  ledUpdateBlinking();
  updateAlarmLed();
  //animateTitle();
  checkCommunicationAlarms();

  // Lógica principal de tempo para solicitar dados
  static unsigned long lastCollectionTime = 0;
  if (millis() - lastCollectionTime >= COLLECTION_INTERVAL_MS) {
    lastCollectionTime = millis();
    requestDataFromSlaves();
  }
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


void setupRTC() {
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




void requestDataFromSlaves() {
  Serial.println("\n=============================================");
  Serial.printf("[%s] - Iniciando novo ciclo de coleta de dados.\n", getTimestamp().c_str());
  Serial.println("=============================================");
  
  for (int i = 0; i < TOTAL_SLAVES; i++) {
    struct_request req;
    req.command = 1; // Comando para "coletar e enviar dados"

    const uint8_t* slaveMac = slaveMacs[i];
    esp_err_t result = esp_now_send(slaveMac, (uint8_t *) &req, sizeof(req));

    if (result == ESP_OK) {
      Serial.printf("--> Solicitacao enviada para o Escravo %d\n", i + 1);
    } else {
      char errorMsg[40];
      snprintf(errorMsg, sizeof(errorMsg), "Erro ao solicitar dados do Escravo %d", i + 1);
      Serial.println(errorMsg);
      logErrorDisplay(errorMsg);
    }
    delay(300); // Um pequeno intervalo entre as solicitações para não sobrecarregar
  }
}



void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  Serial.println("\n=== PACOTE RECEBIDO ===");
  
  // Verificações básicas
  if (!info || !data || len != sizeof(struct_message)) {
    Serial.printf("✗ Pacote inválido (len=%d, esperado=%d)\n", len, sizeof(struct_message));
    return;
  }
  
  struct_message msg;
  memcpy(&msg, data, sizeof(msg));
  
  // Verifica ID válido
  if (msg.id <= 0 || msg.id > TOTAL_SLAVES) {
    Serial.printf("✗ ID inválido: %d\n", msg.id);
    return;
  }
  
  int idx = msg.id - 1;
  
  Serial.printf("✓ Dados recebidos do Slave %d:\n", msg.id);
  Serial.printf("  Voltagem: %.2fV\n", msg.voltage);
  Serial.printf("  Temperatura: %.2f°C\n", msg.temperature);
  
  // Atualiza dados (SEM duplicata por agora)
  boards[idx] = msg;
  boardLastUpdate[idx] = getTimestamp();
  boardLastUpdateMillis[idx] = millis();
  
  // Verifica alarmes
  checkForAlarms(msg, idx);
  
  // ENVIA ACK IMEDIATAMENTE
  Serial.println("Enviando ACK...");
  
  ack_message ack;
  ack.id = msg.id;
  ack.ok = true;
  
  esp_err_t result = esp_now_send(info->src_addr, (uint8_t *)&ack, sizeof(ack));
  
  if (result == ESP_OK) {
    Serial.printf("✓ ACK enviado para Slave %d\n", msg.id);
  } else {
    Serial.printf("✗ ERRO ao enviar ACK: %d\n", result);
  }
  
  Serial.println("======================");
}




/*
void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  // Payload extract
  ledNewData();
  if (len != sizeof(struct_message)) return;  // sanity check
  struct_message msg;
  memcpy(&msg, data, sizeof(msg));

  // Identifies slave by ID
  int idx = msg.id - 1;
  if (idx < 0 || idx >= TOTAL_SLAVES) return;

  static unsigned long lastProcessTime[TOTAL_SLAVES] = {0};
  if (millis() - lastProcessTime[idx] < 1000) { // Ignora se já processou há menos de 1s
    Serial.printf("Ignorando mensagem duplicada do Slave %d\n", msg.id);
    return;
  }
  lastProcessTime[idx] = millis();

  boards[idx] = msg;
  //Register timestamp upon packet received
  DateTime now = rtc.now();
  boardLastUpdate[idx] = getTimestamp();
  boardLastUpdateMillis[idx] = millis();

  //Verificação de alarmes
  checkForAlarms(msg, idx);

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
                info->src_addr[0], info->src_addr[1], info->src_addr[2],
                info->src_addr[3], info->src_addr[4], info->src_addr[5],
                msg.id, msg.voltage, msg.temperature);

      // Send ACK back
  ledNewData();
  ack_message ack = { .id = msg.id, .ok = true };
  esp_err_t result = esp_now_send(info->src_addr, (uint8_t *)&ack, sizeof(ack));


  if (result == ESP_OK) {
    Serial.printf("    --> ACK de confirmacao enviado para %s (ID=%d)\n", macStr, msg.id);
  } else {
    char errorMsgACK[32];
    snprintf(errorMsgACK, sizeof(errorMsgACK), "ERROR send ACK S%d", msg.id);
    Serial.printf(errorMsgACK);
    logErrorDisplay(errorMsgACK);
    ledAnimationBlinking(LED::red, 20);
    addAlarm("Falha ACK Board " + String(msg.id));
  }
}
*/

int findSlaveIndex(const uint8_t *mac) {
  for (int i = 0; i < TOTAL_SLAVES; i++) {
    if (memcmp(mac, slaveMacs[i], 6) == 0) return i;
  }
  return -1;
}


void logToSD(const struct_message &msg) {
  static bool sdError = false;
  if (sdError) return; // Se já falhou, não tenta mais até reset
  
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100); // Reduzido de 200ms
  digitalWrite(SD_CS, LOW);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("SD failed - disabling until reset");
    sdError = true;
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  
  File sdFile = SD.open(SD_FILE, FILE_APPEND);
  if (!sdFile) {
    Serial.println("SD file failed - disabling until reset");
    sdError = true;
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  
  String timestamp = getTimestamp();
  sdFile.printf("%s,%d,%.2f,%.2f\n", timestamp.c_str(), msg.id, msg.voltage, msg.temperature);
  sdFile.close();
  
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

// Função auxiliar para converter DateTime para String
String formatDateTime(DateTime dt) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           dt.year(), dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  return String(buf);
}


// Função para adicionar alarme
void addAlarm(String message) {
  String timestamp = getTimestamp();
  String fullAlarmMsg = timestamp + " " + message;
  
  if (alarmCount < MAX_ALARMS) {
    alarmMsgs[alarmCount++] = fullAlarmMsg;
  } else {
    // Remove o mais antigo e adiciona o novo
    for (int i = 1; i < MAX_ALARMS; i++) {
      alarmMsgs[i - 1] = alarmMsgs[i];
    }
    alarmMsgs[MAX_ALARMS - 1] = fullAlarmMsg;
  }
  
  hasActiveAlarms = true;
  Serial.println("ALARME: " + message);
  
  // Ativa LED vermelho piscando
  //ledAnimationBlinking(LED::red, 50); // 50 piscadas para alarme
}

// Função para limpar alarmes
void clearAlarms() {
  for (int i = 0; i < alarmCount; i++) {
    alarmMsgs[i] = "";
  }
  alarmCount = 0;
  hasActiveAlarms = false;
  Serial.println("Alarmes limpos");
}

// Função para verificar alarmes nos dados recebidos
void checkForAlarms(const struct_message &msg, int boardIndex) {
  char alarmMsg[80];
  
  // Verifica voltagem
  if (msg.voltage < VOLTAGE_MIN || msg.voltage > VOLTAGE_MAX) {
    snprintf(alarmMsg, sizeof(alarmMsg), "Board %d: Voltagem anormal %.2fV", msg.id, msg.voltage);
    addAlarm(String(alarmMsg));
  }
  
  // Verifica temperatura
  if (msg.temperature < TEMP_MIN || msg.temperature > TEMP_MAX) {
    snprintf(alarmMsg, sizeof(alarmMsg), "Board %d: Temperatura anormal %.1f°C", msg.id, msg.temperature);
    addAlarm(String(alarmMsg));
  }
  
  // Verifica valores inválidos típicos de sensores com problema
  if (msg.voltage <= 0 || msg.temperature <= -100) {
    snprintf(alarmMsg, sizeof(alarmMsg), "Board %d: Sensor com falha (V=%.2f T=%.1f)", msg.id, msg.voltage, msg.temperature);
    addAlarm(String(alarmMsg));
  }
}

// Função para verificar timeouts de comunicação
void checkCommunicationAlarms() {
  static unsigned long lastCommCheck = 0;
  if (millis() - lastCommCheck < 60000) return; // Verifica apenas a cada minuto
  lastCommCheck = millis();
  
  for (int i = 0; i < TOTAL_SLAVES; i++) {
    if (boardLastUpdateMillis[i] > 0) {  // Só verifica se já recebeu dados antes
      unsigned long timeSinceUpdate = millis() - boardLastUpdateMillis[i];
      if (timeSinceUpdate > COMM_TIMEOUT_MS) {
        char alarmMsg[60];
        snprintf(alarmMsg, sizeof(alarmMsg), "Board %d: Sem comunicacao %lu min", 
                i + 1, timeSinceUpdate / 60000);
        addAlarm(String(alarmMsg));
      }
    }
  }
}


String getSlaveStatus(int slaveId) {
  int idx = slaveId - 1;
  if (idx < 0 || idx >= TOTAL_SLAVES) return "";
  
  String status = "";
  struct_message board = boards[idx];
  
  // Timeout de comunicação
  if (boardLastUpdateMillis[idx] > 0) {
    unsigned long timeSinceUpdate = millis() - boardLastUpdateMillis[idx];
    if (timeSinceUpdate > COMM_TIMEOUT_MS) {
      status += "X"; // Timeout
    }
  }
  
  // Valores fora do range
  if (board.voltage <= VOLTAGE_MIN || board.voltage > VOLTAGE_MAX) {
    status += "V"; // Voltage problem
  }
  
  if (board.temperature < TEMP_MIN || board.temperature > TEMP_MAX) {
    status += "T"; // Temperature problem
  }
  
  return status.length() > 0 ? String(slaveId) + ":" + status : "";
}


bool hasAnySystemProblem() {
  for (int i = 1; i <= TOTAL_SLAVES; i++) {
    String status = getSlaveStatus(i);
    if (status.length() > 0) {
      return true; // Encontrou pelo menos um problema
    }
  }
  return false; // Sistema OK
}


void updateAlarmLed() {
  static bool alarmLedState = false;
  static unsigned long lastAlarmBlink = 0;
  
  if (hasAnySystemProblem()) {
    // Pisca LED vermelho a cada 1 segundo
    if (millis() - lastAlarmBlink >= 1000) {
      alarmLedState = !alarmLedState;
      digitalWrite(LED::red, alarmLedState ? HIGH : LOW);
      lastAlarmBlink = millis();
    }
  } else {
    // Sistema OK - LED vermelho apagado
    digitalWrite(LED::red, LOW);
    alarmLedState = false;
  }
}