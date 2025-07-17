#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  Serial.println();  
  WiFi.mode(WIFI_STA); 
}

void loop() {
  Serial.print("MAC WiFi STA: ");
  Serial.println(WiFi.macAddress());
  delay(10000);
}