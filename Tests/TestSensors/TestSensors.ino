#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ADS1115_WE.h>
#include <Wire.h>

#define I2C_ADDRESS 0x48  // ADS1115
#define NODE_ID 4

namespace TMP {
  const int tmp_pin = 23;  // Mude para 21 ou 22 se mudou o pino
  bool tmp_found = false;
}

// Create temp sensor object and temporary address
OneWire oneWire(TMP::tmp_pin);
DallasTemperature tmp_sensor(&oneWire);
DeviceAddress tmp_add;

// Create ADC object
ADS1115_WE adc(I2C_ADDRESS);

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("================================");
  Serial.printf("TESTE DOS SENSORES - SLAVE %d\n", NODE_ID);
  Serial.println("================================");
  
  // Desliga WiFi para economizar energia
  WiFi.mode(WIFI_OFF);
  
  initSensors();
  
  Serial.println("\nIniciando leituras a cada 10 segundos...\n");
}

void loop() { 
  static unsigned long lastReading = 0;
  
  if (millis() - lastReading >= 10000) { // 10 segundos
    lastReading = millis();
    
    Serial.println("--- NOVA LEITURA ---");
    Serial.printf("Tempo: %lu ms\n", millis());
    
    float voltage = getVoltage();
    float temperature = getTemperature();
    
    Serial.printf("RESULTADO: V=%.3fV | T=%.2f°C\n", voltage, temperature);
    
    // Status dos sensores
    Serial.printf("Status ADS1115: %s\n", isADS1115Available() ? "OK" : "ERRO");
    Serial.printf("Status DS18B20: %s\n", TMP::tmp_found ? "OK" : "ERRO");
    Serial.println("-------------------\n");
  }
  
  delay(100);
}

void initSensors(){
  Serial.println("Inicializando sensores...");
  
  // Init I2C
  Wire.begin();
  delay(100);
  
  // Test ADS1115
  Serial.print("Testando ADS1115... ");
  if (isADS1115Available()) {
    Serial.println("ENCONTRADO!");
    if(adc.init()){
      adc.setVoltageRange_mV(ADS1115_RANGE_6144);
      adc.setMeasureMode(ADS1115_CONTINUOUS);
      Serial.println("ADS1115 configurado com sucesso");
    } else {
      Serial.println("ERRO na configuração do ADS1115");
    }
  } else {
    Serial.println("NÃO ENCONTRADO!");
  }

  // Test DS18B20
  Serial.print("Testando DS18B20... ");
  tmp_sensor.begin();
  delay(100);
  
  if (tmp_sensor.getAddress(tmp_add, 0)){
    TMP::tmp_found = true;
    Serial.println("ENCONTRADO!");
    Serial.print("Endereço: ");
    for (uint8_t i = 0; i < 8; i++) {
      Serial.printf("%02X ", tmp_add[i]);
    }
    Serial.println();
  } else {
    Serial.println("NÃO ENCONTRADO!");
    TMP::tmp_found = false;
  }
  
  Serial.println("Inicialização concluída!\n");
}

bool isADS1115Available() {
  Wire.beginTransmission(I2C_ADDRESS);
  return (Wire.endTransmission() == 0);
}

float getVoltage(){
  if (!isADS1115Available()) {
    Serial.println("  ADS1115 não disponível!");
    return 0.0;
  }
  
  const int samples = 3;
  float sum = 0.0;
  int validReadings = 0;
  
  adc.setCompareChannels(ADS1115_COMP_0_1); // Medição single-ended A0-GND
  delay(100); // Tempo para estabilizar
  
  for (int i = 0; i < samples; i++){
    float reading = adc.getResult_V();
    if (!isnan(reading) && reading >= 0) {
      sum += reading;
      validReadings++;
      Serial.printf("  Leitura %d: %.3fV\n", i+1, reading);
    } else {
      Serial.printf("  Leitura %d: INVÁLIDA\n", i+1);
    }
    delay(100);
  }
  
  float voltage = (validReadings > 0) ? sum / validReadings : 0.0;
  Serial.printf("  Voltagem final: %.3fV (%d/%d leituras válidas)\n", 
                voltage, validReadings, samples);
  
  return voltage;
}

float getTemperature(){
  if (!TMP::tmp_found){
    Serial.println("  DS18B20 não disponível!");
    return -127.0;
  }
  
  const int samples = 3;
  float sum = 0.0;
  int validReadings = 0;
  
  for (int i = 0; i < samples; i++){
    Serial.printf("  Solicitando temperatura %d...", i+1);
    
    tmp_sensor.requestTemperatures();
    delay(800); // Tempo para conversão (750ms + margem)
    
    float tempC = tmp_sensor.getTempC(tmp_add);
    
    if (tempC != -127.0 && tempC != 85.0 && !isnan(tempC)){
      sum += tempC;
      validReadings++;
      Serial.printf(" %.2f°C\n", tempC);
    } else {
      Serial.printf(" ERRO (%.2f°C)\n", tempC);
    }
    
    delay(200);
  }
  
  float temperature = (validReadings > 0) ? sum / validReadings : -127.0;
  Serial.printf("  Temperatura final: %.2f°C (%d/%d leituras válidas)\n", 
                temperature, validReadings, samples);
  
  return roundf(temperature * 100.0) / 100.0;
}

// Função de diagnóstico
void printDiagnostics() {
  Serial.println("\n=== DIAGNÓSTICOS ===");
  Serial.printf("Heap livre: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Pino temperatura: %d\n", TMP::tmp_pin);
  Serial.printf("I2C SDA: %d, SCL: %d\n", SDA, SCL);
  Serial.println("===================\n");
}