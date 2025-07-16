#include <esp_now.h>
#include <WiFi.h>
#include <SD.h>
#include <FS.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "LedAnimations.h"

#define BOARD_STATUS_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 min

#define TOTAL_SENDERS 9
#define NODE_ID_UNKNOWN 0

// Display pins
#define TFT_CS  15
#define TFT_RST 4
#define TFT_DC  2

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
  {0xAA,0xBB,0xCC,0x11,0x22,0x33},  // Sender 1
  {0x14,0x33,0x5C,0x02,0xED,0xA0},  // Sender 2
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

// Structure example to receive data. Must match the sender structure
typedef struct struct_message {
  int id;
  float voltage;
  float temperature;
} struct_message;

typedef struct ack_message {
  int id;
  bool ok;
} ack_message;

struct_message boards[TOTAL_SENDERS];

// Init ST7735 display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


void setup() {
  Serial.begin(115200);
  ledTurnOff();

  // Configure ESP-NOW
  setupESPNow();
  
  // Init SD module
  if (!initSD()) {
    Serial.println("ERROR initializing SD at init!");
    ledAnimationBlinking(LED::red, 20);
  }
}
 
void loop() {
  ledUpdateNewData();
  ledUpdateBlinking();
  boardStatus();
}


void setupESPNow() {
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR initializing ESP-NOW");
    ledAnimationBlinking(LED::red, 20);
    return;
  }

  // Register for Recv CB to get the status of packets  
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("Successfully initialized ESP-NOW");
  ledAnimationBlinking(LED::green, 5);
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
    Serial.println("ERROR: initializing SD!");
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