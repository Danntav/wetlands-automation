#include <WiFi.h>
#include <esp_now.h>
#include <driver/rtc_io.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define uS_TO_S_FACTOR 1000000ULL
//#define TIME_TO_SLEEP  30*60
#define TIME_TO_SLEEP  3000
#define ACK_TIMEOUT_MS 2000

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex
#define USE_EXT0_WAKEUP          0               // 1 = EXT0 wakeup, 0 = EXT1 wakeup
#define WAKEUP_GPIO              GPIO_NUM_33     // Only RTC IO are allowed - ESP32 Pin example

#define NODE_ID 1

// Master's MAC
uint8_t masterMacAddress[] = {0x14, 0x33, 0x5C, 0x02, 0xED, 0x6C};

// Structure to send data. Must match the receiver structure
typedef struct struct_message {
    int id; // Unique for each sender
    float voltage;
    float temperature; 
} struct_message;

typedef struct {
  int id;
  bool ok;
} ack_message;

// Create a struct_message called myData
struct_message myData;

// Create peer interface
esp_now_peer_info_t peerInfo;

namespace US{
  const int trig = 26;
  const int echo = 27;
  const long limit = 20; // nível crítico em cm
}

namespace TMP{
  const int one_wire = 23;
}

namespace LED{
  const int white = 25;
  const int red = 18;
  const int green = 19;
}

RTC_DATA_ATTR int bootCount = 0;

volatile bool ackReceived = false;

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  pinMode(LED::white, OUTPUT);
  pinMode(LED::red, OUTPUT);
  pinMode(LED::green, OUTPUT);

  ledUSLimit();

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  setupESPNow();
  configureWakeups();

  esp_sleep_wakeup_cause_t wakeup_reason= esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
    levelUS();
  }
  else if (ESP_SLEEP_WAKEUP_TIMER){
    measureSensors();
    sendPayloadAndWaitAck();
  }

  else{
    Serial.print("First Boot");
  }
  
  esp_deep_sleep_start();
}


void loop() {
  // EMPTY
}


void setupESPNow(){
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for Send CB to get the status of Transmitted packet
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onAckRecv);
  
  // Register peer
  memcpy(peerInfo.peer_addr, masterMacAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

// callback when data is sent
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


void onAckRecv(const uint8_t *mac, const uint8_t *data, int len) {
  ack_message *ack = (ack_message *)data;
  if (ack->id == NODE_ID && ack->ok) {
    ackReceived = true;
  }
}

void sendPayloadAndWaitAck() {
  ackReceived = false;
  esp_now_send(masterMacAddress, (uint8_t *)&myData, sizeof(myData));

  unsigned long start = millis();
  while (!ackReceived && millis() - start < ACK_TIMEOUT_MS) {
    // Aguarda o callback setar ackReceived
    delay(10);
  }

  if (ackReceived) {
    Serial.println("ACK recebido do mestre");
  } else {
    Serial.println("Nenhum ACK (timeout)");
  }
}

void measureSensors(){
  int rawVoltage = analogRead(26); 
  float voltage = (rawVoltage / 4095.0) * 3.3;
  myData.voltage = voltage * 100;  // Envia em centivolts

  // @@IMPLEMENTAR TEMP
  myData.temperature = random(200, 300);  // Exemplo: 20.0°C a 30.0°C
  myData.id = NODE_ID;
}

void configureWakeups() {
  // Configure wakeup via button (EXT0)
  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 1);
  rtc_gpio_pullup_dis(WAKEUP_GPIO);
  rtc_gpio_pulldown_en(WAKEUP_GPIO);

  // Configure wakeup source every "TIME_TO_SLEEP" seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep now");

  delay(1000);
  Serial.flush(); 
}


void levelUS() {
  float distance_cm = 0;

  while (distance_cm > US::limit){
    pinMode(US::echo, INPUT);
    pinMode(US::trig, OUTPUT);
    digitalWrite(US::trig, LOW);
    delayMicroseconds(2);
    digitalWrite(US::trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(US::trig, LOW);

    long duration = pulseIn(US::echo, HIGH, 30000); // timeout 30 ms
    float distance_cm = duration * 0.0343 / 2;

    Serial.printf("Nível: %.1f cm\n", distance_cm);
  }

  ledUSLimit();

}

void ledUSLimit(){

  for (int i = 0; i<40; i++){
    
    Serial.println("blinking led");
    digitalWrite(LED::white, HIGH);
    delay(250);
    digitalWrite(LED::white, LOW);
    delay(250);
  }

  digitalWrite(LED::white, 0);
}

// void ledSendingInfo(){};
// void ledAckOK(){};
// void ledDeepSleep(){};