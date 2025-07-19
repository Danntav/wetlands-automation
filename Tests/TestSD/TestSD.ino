#include <SPI.h>
#include <SD.h>

#define SD_SCK  14
#define SD_MISO 13
#define SD_MOSI 12
#define SD_CS   27

void setup() {
  Serial.begin(115200);
  delay(1000); // Aguardar inicialização do serial

  Serial.println("Iniciando teste SD...");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) { // Usar SPI padrão
    Serial.println("ERROR: SD init failed!");
    return;
  }
  Serial.println("SD initialized successfully.");

  if (!SD.exists("/test.txt")) {
    File file = SD.open("/test.txt", FILE_WRITE);
    if (file) {
      file.println("Test file created!");
      file.close();
      Serial.println("Created /test.txt");
    } else {
      Serial.println("ERROR: Failed to create /test.txt");
    }
  }

  File file = SD.open("/test.txt", FILE_READ);
  if (file) {
    Serial.println("Reading /test.txt:");
    while (file.available()) {
      Serial.write(file.read());
    }
    file.close();
  } else {
    Serial.println("ERROR: Failed to read /test.txt");
  }
}

void loop() {
  delay(100); // Evitar WDT reset
}