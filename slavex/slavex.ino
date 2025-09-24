#include <WiFi.h>
#include <esp_now.h>
#include <driver/rtc_io.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ADS1115_WE.h>
#include <Wire.h>
#include "LedAnimations.h"


#define ACK_TIMEOUT_MS 2000
#define MAX_RETRIES 3     //@@implementation for resend payload after ACK NOK
#define I2C_ADDRESS 0x48  // ADS1115
#define NODE_ID 1   //unique for each Slave[x]

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
  // O loop agora só precisa cuidar de tarefas secundárias, como as animações de LED
  ledUpdateBlinking();
  ledUpdateNewData();
  delay(10);
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

// callback when data is sent
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery FAIL");
}


// NOVO: Função que encapsula a coleta e o envio
void collectAndSendData() {
  measureSensors();
  sendPayloadWithRetries();
}


// MODIFICADO: Callback unificado para receber tanto solicitações quanto ACKs
void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  // Se o tamanho dos dados for o de um ACK, trate como um ACK
  if (len == sizeof(ack_message)) {
    ack_message ack;
    memcpy(&ack, incomingData, sizeof(ack));
    if (ack.id == NODE_ID && ack.ok) {
      ackReceived = true;
    }
  }
  // Se o tamanho for o de uma solicitação, inicie a coleta
  else if (len == sizeof(struct_request)) {
    struct_request req;
    memcpy(&req, incomingData, sizeof(req));

    // Se o comando for para coletar dados (ex: 1)
    if (req.command == 1) {
      Serial.println("Solicitacao do Mestre recebida. Coletando e enviando dados...");
      collectAndSendData();
    }
  }
}

// Função de envio com lógica de reenvio
void sendPayloadWithRetries() {
  bool success = false;

  for (int i = 0; i < MAX_RETRIES; i++) {
    ackReceived = false;
    esp_now_send(masterMacAddress, (uint8_t *)&myData, sizeof(myData));

    unsigned long start = millis();
    while (!ackReceived && millis() - start < ACK_TIMEOUT_MS) {
      // Aguarda o callback setar ackReceived = true
      ledNewData();
      ledUpdateNewData();
      delay(20);
    }

    if (ackReceived) {
      Serial.println("ACK OK recebido do mestre!");
      ledAnimationBlinking(LED::green, 5);
      success = true;
      break; // Sai do loop se o ACK foi recebido
    } else {
      Serial.printf("ACK NOK (timeout). Tentativa %d de %d...\n", i + 1, MAX_RETRIES);
      ledAnimationBlinking(LED::red, 1);
    }
  }

  if (!success) {
    Serial.println("------------------------------------");
    Serial.println("FALHA AO ENVIAR DADOS APOS 3 TENTATIVAS.");
    Serial.println("------------------------------------");
    ledAnimationBlinking(LED::red, 10);
  }
}


void initSensors(){
  // Init voltage adc - ADS1115
  Wire.begin();
  if(!adc.init()){
    Serial.println("ERROR ADS1115 not found!");
  }
  adc.setVoltageRange_mV(ADS1115_RANGE_6144);
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


void measureSensors(){
  float voltage = getVoltage();
  float temperature = getTemperature();

  myData.id = NODE_ID;
  myData.voltage = voltage;
  myData.temperature = temperature;
}


float getVoltage(){
  // Reading voltage - ADS1115
  float voltage = 0.00;
  const int samples = 5;
  float sum = 0.0;
  adc.setCompareChannels(ADS1115_COMP_0_GND);
  for (int i = 0; i < samples; i++){
    sum += adc.getResult_V();
    delay(50);
  }
  voltage = sum / samples;
  Serial.print("Pin A0: ");
  Serial.print(voltage, 3);
  
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