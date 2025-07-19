#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SD.h>

#define TFT_CS  15
#define TFT_RST 4
#define TFT_DC  2
#define SD_CS   5
#define SD_FILE "/data.csv"

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Iniciando teste display e SD no VSPI...");

  // Inicializar VSPI
  SPI.begin(18, 19, 23, TFT_CS); // SCK=18, MISO=19, MOSI=23
  SPI.setFrequency(1000000); // 1 MHz para estabilidade

  // Inicializar display
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH); // Desselecionar display
  digitalWrite(SD_CS, HIGH); // Desselecionar SD
  delay(100);
  digitalWrite(TFT_CS, LOW); // Ativar display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("Teste Display e SD");
  Serial.println("Display initialized.");
  digitalWrite(TFT_CS, HIGH); // Desselecionar display

  // Inicializar SD
  digitalWrite(SD_CS, LOW); // Ativar SD
  delay(100);
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR init SD!");
    tft.setCursor(0, 20);
    digitalWrite(TFT_CS, LOW);
    tft.println("ERROR init SD!");
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  Serial.println("SD successfully initialized.");
  tft.setCursor(0, 20);
  digitalWrite(TFT_CS, LOW);
  tft.println("SD OK");
  digitalWrite(TFT_CS, HIGH);

  // Criar/ler arquivo
  digitalWrite(SD_CS, LOW);
  if (!SD.exists(SD_FILE)) {
    File f = SD.open(SD_FILE, FILE_WRITE);
    if (f) {
      f.println("timestamp,id,voltage,temperature");
      f.close();
      Serial.println("Created /data.csv");
      digitalWrite(TFT_CS, LOW);
      tft.println("Created /data.csv");
      digitalWrite(TFT_CS, HIGH);
    } else {
      Serial.println("ERROR: Failed to create /data.csv");
      digitalWrite(TFT_CS, LOW);
      tft.println("ERROR: Failed to create /data.csv");
      digitalWrite(TFT_CS, HIGH);
    }
  }
  File f = SD.open(SD_FILE, FILE_READ);
  if (f) {
    Serial.println("Reading /data.csv:");
    while (f.available()) {
      Serial.write(f.read());
    }
    f.close();
  } else {
    Serial.println("ERROR: Failed to read /data.csv");
    digitalWrite(TFT_CS, LOW);
    tft.println("ERROR: Failed to read /data.csv");
    digitalWrite(TFT_CS, HIGH);
  }
  digitalWrite(SD_CS, HIGH);
  SPI.end();
}

void loop() {
  delay(10); // Evitar WDT
}