#include <WiFi.h>
#include <esp_now.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ADS1115_WE.h>
#include <Wire.h>
#include "LedAnimations.h"

#define ACK_TIMEOUT_MS (5UL * 1000UL)
#define MAX_RETRIES 3     //resend payload after ACK NOK
#define I2C_ADDRESS 0x48  // ADS1115
#define NODE_ID 9   //unique for each Slave[x]

// Master's MAC
uint8_t masterMacAddress[] = {0x3C, 0x8A, 0x1F, 0x5E, 0x16, 0x48};

// Structure to send data. Must match the receiver structure
typedef struct struct_message {
  int id; // Unique for each slave
  float voltage;
  float temperature; 
} struct_message;

typedef struct ack_message {
  int id;
  bool ok;
} ack_message;

// Estrutura para receber a solicitação do Mestre.
typedef struct struct_request {
  int command; // Um comando simples, ex: 1 para "coletar e enviar dados"
} struct_request;

namespace TMP {
  const int tmp_pin = 23;
  bool tmp_found = false;
}

// Variáveis de controle para ACK
volatile bool dataReadyToSend = false; 
volatile bool ackReceived = false;

// Create a struct_message called myData
struct_message myData;

// Create peer interface
esp_now_peer_info_t peerInfo;

// Create temp sensor object and temporary address
OneWire oneWire(TMP::tmp_pin);
DallasTemperature tmp_sensor(&oneWire);
DeviceAddress tmp_add;

// Create ADC object
ADS1115_WE adc(I2C_ADDRESS);

void setup() {
  Serial.begin(115200);
   
  pinMode(LED::yellow, OUTPUT);
  pinMode(LED::red, OUTPUT);
  pinMode(LED::green, OUTPUT);

  setupESPNow();
  ledTurnOff();
  initSensors();
}


void loop() { 
  if (dataReadyToSend){
    sendData();
    dataReadyToSend = false;
  }
  ledUpdateBlinking();
  ledUpdateNewData();
  delay(10);
  // Verificar se ESP-NOW ainda está ativo
  static unsigned long lastHealthCheck = 0;
  if (millis() - lastHealthCheck > 30000) { // A cada 30s
    if (WiFi.getMode() != WIFI_STA) {
      Serial.println("WiFi mode changed! Reinitializing...");
      setupESPNow();
    }
    lastHealthCheck = millis();
  }
}


void setupESPNow(){
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR init ESP-NOW");
    return;
  }
  
  // Register for Send and Recv CB to get the status of packets
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("Successfully init ESP-NOW");
  
  // Register peer
  memcpy(peerInfo.peer_addr, masterMacAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("FAILED to add peer");
    return;
  }
}

// UPDATED: callback when data is sent - new signature for ESP32 core v3.x
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("\rLast Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery FAIL");
}


// Callback unificado para receber tanto solicitações quanto ACKs - new signature
void onDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  // ACK recebido
  if (len == sizeof(ack_message)) {
    ack_message ack;
    memcpy(&ack, incomingData, sizeof(ack));
    
    if (ack.id == NODE_ID && ack.ok) {
      ackReceived = true;
      Serial.printf("ACK OK recebido! (ID=%d)\n", ack.id);
    }
    return;
  }
  
  // Comando do Master
  if (len == sizeof(struct_request)) {
    struct_request req;
    memcpy(&req, incomingData, sizeof(req));
    
    if (req.command == 1) {
      Serial.println("\n>>> COMANDO RECEBIDO! Enviando dados...");
      
      // Coleta dados imediatamente
      myData.id = NODE_ID;
      myData.voltage = getVoltage();
      myData.temperature = getTemperature();
      
      Serial.printf("Dados: ID=%d, V=%.2f, T=%.2f\n", myData.id, myData.voltage, myData.temperature);
      
      // Envia com retry simples
      dataReadyToSend = true;
    }
  }
}


void sendData() {
  for (int i = 1; i <= MAX_RETRIES; i++) {
    Serial.printf("\n--- Envio %d/%d ---\n", i, MAX_RETRIES);
    ackReceived = false;
    
    // Envia dados
    esp_err_t result = esp_now_send(masterMacAddress, (uint8_t *)&myData, sizeof(myData));
    
    if (result != ESP_OK) {
      Serial.printf("✗ Erro esp_now_send: %d\n", result);
      delay(1000);
      continue;
    }
    
    Serial.println("Pacote enviado, aguardando ACK...");
    
    // Espera ACK
    unsigned long start = millis();
    while (!ackReceived && (millis() - start) < ACK_TIMEOUT_MS) {
      delay(100);
      yield();
    }
    
    if (ackReceived) {
      Serial.println("SUCESSO! Comunicacao OK!");
      ledAnimationBlinking(LED::green, 3);
      return;
    } else {
      Serial.printf("Timeout apos %lu ms\n", millis() - start);
      ledAnimationBlinking(LED::red, 1);
      
      if (i < MAX_RETRIES) {
        delay(500); // Pausa entre tentativas
      }
    }
  }
  
  Serial.println("FALHOU apos todas as tentativas!");
  ledAnimationBlinking(LED::red, 5);
}


void initSensors(){
  // Init voltage adc - ADS1115
  Wire.begin();
  if(!adc.init()){
    Serial.println("ERROR ADS1115 not found!");
  }
  adc.setVoltageRange_mV(ADS1115_RANGE_1024);
  adc.setMeasureMode(ADS1115_CONTINUOUS);

  // Init temp sensor - DS18B20
  tmp_sensor.begin();
  if (tmp_sensor.getAddress(tmp_add, 0)){
    TMP::tmp_found = true;
  } else {
    Serial.println("DS18B20 not found at init!");
    TMP::tmp_found = false;
  }
}


float getVoltage(){
  // Reading voltage - ADS1115
  float voltage = 0.00;
  const int samples = 5;
  float sum = 0.0;
  adc.setCompareChannels(ADS1115_COMP_0_1);
  for (int i = 0; i < samples; i++){
    sum += adc.getResult_V();
    delay(50);
  }
  voltage = sum / samples;
  Serial.print("Pin A0: ");
  Serial.print(voltage, 4);
  
  return voltage; // alternative: getResult_mV for Millivolt
}


float getTemperature(){
  // Reading temp sensor - DS18B20
  if (!TMP::tmp_found){
    return -127.0;
  }
  const int samples = 5;
  int readings = 0;
  float sum = 0.0;
  for (int i = 0; i < samples; i++){
    tmp_sensor.requestTemperatures();
    float tempC = tmp_sensor.getTempC(tmp_add);
    if (tempC != -127.0 && tempC != 85.0){
        sum += tempC;
        readings++;
      }
    delay(300);
  }
  float average = readings > 0 ? sum / readings : -127.0;
  Serial.printf("Temp average: %.2f°C (%d valid readings)\n", average, readings);
  
  return roundf(average * 100.0) / 100.0;
}