#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  Serial.println();  
  WiFi.mode(WIFI_STA); 
}

void loop() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.println("MAC WiFi STA: ");
  Serial.println(WiFi.macAddress());
  Serial.print("{");
  for (int i = 0; i < 6; i++) {
    Serial.printf("0x%02X", mac[i]);
    if (i < 5) {
      Serial.print(", ");
    }
  }
  Serial.println("};");

  
  
  delay(10000);
}