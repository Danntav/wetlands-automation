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

#define COLLECTION_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 min
#define NODE_ID_UNKNOWN 0

String boardLastUpdate[TOTAL_SLAVES];
unsigned long boardLastUpdateMillis[TOTAL_SLAVES];

// Buffer para armazenar os dados recebidos antes de gravar no SD
struct_message dataBuffer[TOTAL_SLAVES];
int bufferCount = 0;
// Flag para controlar o ciclo de gravação
bool isCollectionCycleActive = false;
unsigned long requestCycleStartTime = 0;

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
  { 0x6C, 0xC8, 0x40, 0x33, 0x76, 0x50 },  // Slave 9
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
  Serial.println("\n\n--- FIRMWARE V3.0 - CORRECAO SD ---");
  ledTurnOff();
  setupLeds();
  setupDisplay();
  delay(1500);

  setupRTC();
  delay(1000);

  setupSD();

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
  handleJoystick();
  ledUpdateNewData();
  ledUpdateBlinking();
  updateAlarmLed();
  animateTitle();
  checkCommunicationAlarms();

  // Lógica principal de tempo para SOLICITAR dados
  if (!isCollectionCycleActive && (millis() - requestCycleStartTime >= COLLECTION_INTERVAL_MS)) {
    requestDataFromSlaves();
    requestCycleStartTime = millis(); // Marca o início do ciclo de coleta
    isCollectionCycleActive = true;
    Serial.println("Ciclo de coleta iniciado. Aguardando respostas...");
  }

  // Lógica para GRAVAR os dados após um tempo de espera
  if (isCollectionCycleActive && (millis() - requestCycleStartTime > 15000)) { // Ex: espera 10s
    writeBufferToSD();
    bufferCount = 0;
    isCollectionCycleActive = false; // Prepara para o próximo ciclo
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
  delay(50);
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


void setupSD() {
  Serial.println("Initializing SD card...");
  
  // Garante que outros dispositivos SPI estão desativados
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  delay(50);
  
  // Inicia SPI com os pinos corretos para o SD
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(50);
  
  Serial.print("Testing SD card on CS pin: ");
  Serial.println(SD_CS);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization FAILED!");
    logErrorDisplay("SD Card failed!");
    sdInitialized = false;
    
    // Tenta listar arquivos para diagnóstico
    Serial.println("Attempting to list files on SD card...");
    File root = SD.open("/");
    if (root) {
      root.rewindDirectory();
      Serial.println("SD card content:");
      while (true) {
        File entry = root.openNextFile();
        if (!entry) break;
        Serial.print("  ");
        Serial.println(entry.name());
        entry.close();
      }
      root.close();
    }
  } else {
    Serial.println("SD Card initialized successfully.");
    sdInitialized = true;
    
    // Verifica se o arquivo existe, se não, cria cabeçalho
    if (!SD.exists(SD_FILE)) {
      File sdFile = SD.open(SD_FILE, FILE_WRITE);
      if (sdFile) {
        sdFile.println("Date,Time,BoardID,Voltage,Temperature");
        sdFile.close();
        Serial.println("Created new data file with header");
      }
    }
    Serial.println("SD card is ready for writing.");
  }
  SPI.end();
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
      Serial.printf("Solicitacao enviada para o Escravo \[%d\]\n", i + 1);
      ledNewData();
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
  Serial.println("PACOTE RECEBIDO --> Processando mensagem...");
  ledNewData();
  // Validações rápidas
  if (len != sizeof(struct_message)) return;
  struct_message msg;
  memcpy(&msg, data, sizeof(msg));
  if (msg.id <= 0 || msg.id > TOTAL_SLAVES) return;

  // Filtro para evitar processamento duplicado
  int idx = msg.id - 1;
  if (idx < 0 || idx >= TOTAL_SLAVES) return;
  static unsigned long lastProcessTime[TOTAL_SLAVES] = {0};
  if (millis() - lastProcessTime[idx] < 1000) { // Ignora se já processou há menos de 1s
    Serial.printf("Ignorando mensagem duplicada do Slave %d\n", msg.id);
    return;
  }
  lastProcessTime[idx] = millis();
  
  // Envia o ACK imediatamente
  ack_message ack = { .id = msg.id, .ok = true };
  esp_now_send(info->src_addr, (uint8_t *)&ack, sizeof(ack));
  Serial.println("Enviando ACK...");
    
  // Atualiza dados (SEM duplicata por agora)
  boards[idx] = msg;
  boardLastUpdate[idx] = getTimestamp();
  boardLastUpdateMillis[idx] = millis();
  
  // Verifica alarmes
  checkForAlarms(msg, idx);

  // Adiciona a mensagem ao buffer se houver espaço
  if (bufferCount < TOTAL_SLAVES) {
    dataBuffer[bufferCount] = msg;
    bufferCount++;
    ledNewData(); // Pisca o LED para feedback visual
  }
  // Esta parte é ótima para depuração e não atrapalha, pois o ACK já foi enviado.
  Serial.printf("\n=== PACOTE RECEBIDO (Slave %d) ===\n", msg.id);
  Serial.printf("  Voltagem: %.4fV\n", msg.voltage);
  Serial.printf("  Temperatura: %.2f°C\n", msg.temperature);
  Serial.println("  -> ACK enviado, dados atualizados e adicionados ao buffer de gravacaoo.");
  Serial.println("==================================");
}


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
  delay(50);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100);
  digitalWrite(SD_CS, LOW);
  delay(50);
  
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
  snprintf(buf, sizeof(buf), "%02d-%02d-%04d %02d:%02d:%02d",
           now.day(), now.month(), now.year(),
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
  
  if (board.temperature <= TEMP_MIN || board.temperature > TEMP_MAX) {
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

void writeBufferToSD() {
  if (bufferCount == 0) {
    Serial.println("Buffer vazio, nada para gravar no SD.");
    return;
  }

  digitalWrite(TFT_CS, HIGH); // Garante que o display está desativado
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  digitalWrite(SD_CS, LOW);  // Ativa o pino de Chip Select do SD Card
  delay(10); // Pequena pausa para estabilizar

  if (!SD.begin(SD_CS)) {
    Serial.println("FALHA: Nao foi possivel inicializar o SD Card na operacao de escrita.");
    sdInitialized = false;
    // Limpa e sai, liberando o barramento
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  sdInitialized = true; // Se chegou aqui, a inicialização funcionou.
  
  File sdFile = SD.open(SD_FILE, FILE_APPEND);
  //File sdFile = SD.open("/testfile.txt", FILE_APPEND);
  if (!sdFile) {
    Serial.println("Failed to open file for writing");
    logErrorDisplay("Failed open SD file");
    //sdInitialized = false;
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }

  // Grava todos os dados do buffer
  for (int i = 0; i < bufferCount; i++) {
    String timestamp = getTimestamp();
    String date = timestamp.substring(0,10);
    String time = timestamp.substring(11);
    String dataLine = date + "," + time + "," + String(dataBuffer[i].id) + "," + 
                     String(dataBuffer[i].voltage, 4) + "," + 
                     String(dataBuffer[i].temperature, 2);
    
    sdFile.println(dataLine);
    Serial.println("Written: " + dataLine);
  }
  
  sdFile.close();
  Serial.printf("%d registros gravados com sucesso!\n", bufferCount);

  for(int i=0; i<10; i++){
    digitalWrite(LED::green, HIGH);
    delay(80);
    digitalWrite(LED::green, LOW);
    delay(80);
  }

  // Limpa o buffer após gravação bem-sucedida
  bufferCount = 0;
  
  // Libera o barramento SPI
  digitalWrite(SD_CS, HIGH);
  SPI.end();
  
  // Reativa o display se necessário
  setupDisplayForTFT();
}

void setupDisplayForTFT() {
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_CS, LOW);
  delay(50);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  digitalWrite(TFT_CS, HIGH);
  SPI.end();
}