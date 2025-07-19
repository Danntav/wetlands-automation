#include <Wire.h>
#include <RTClib.h>

#define RTC_SDA 21
#define RTC_SCL 22

RTC_DS1307 rtc;

void setup() {
  Serial.begin(115200);
  delay(1000); // Aguardar inicialização do serial

  Serial.println("Iniciando teste RTC...");
  Wire.begin(RTC_SDA, RTC_SCL);
  scanI2C(); // Verificar dispositivos no barramento I2C

  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found!");
    return;
  }
  if (!rtc.isrunning()) {
    Serial.println("ERROR: RTC not running");
    rtc.adjust(DateTime(2025, 7, 17, 20, 31, 0)); // Ajustar para data inicial
    Serial.println("RTC adjusted to 2025-07-17 20:31:00");
  } else {
    Serial.println("RTC successfully initialized.");
  }
}

void loop() {
  if (rtc.isrunning()) {
    String timestamp = getTimestamp();
    Serial.println("Timestamp: " + timestamp);
  } else {
    Serial.println("ERROR: RTC not running");
  }
  delay(1000); // Exibir a cada 1 segundo
}

String getTimestamp() {
  if (!rtc.isrunning()) {
    Serial.println("ERROR: RTC not running");
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