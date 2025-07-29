#include <WiFi.h>
#include <esp_now.h>
#include <driver/rtc_io.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ADS1115_WE.h>
#include <Wire.h>
#include "LedAnimations.h"

#define uS_TO_S_FACTOR 1000000ULL
//#define TIME_TO_SLEEP  30*60
#define TIME_TO_SLEEP  30
#define ACK_TIMEOUT_MS 2000
//#define MAX_RETRIES 3     //@@implementation for resend payload after ACK NOK

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex
#define USE_EXT0_WAKEUP          0               // 1 = EXT0 wakeup, 0 = EXT1 wakeup
#define WAKEUP_GPIO              GPIO_NUM_33     // Only RTC IO are allowed

#define I2C_ADDRESS 0x48  // ADS1115

#define NODE_ID 1

// Master's MAC
uint8_t masterMacAddress[] = {0x14, 0x33, 0x5C, 0x02, 0xED, 0x6C};

// Structure to send data. Must match the receiver structure
typedef struct struct_message {
  int id; // Unique for each slave
  float voltage;
  float temperature; 
} struct_message;

typedef struct {
  int id;
  bool ok;
} ack_message;

namespace US {
  const int trig = 26;
  const int echo = 27;
  const int limit = 20; // limit level in cm
}

namespace TMP {
  const int tmp_pin = 23;
  bool tmp_found = false;
}

RTC_DATA_ATTR int bootCount = 0;

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

  // Count boot resets
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  setupESPNow();
  configureWakeups();

  esp_sleep_wakeup_cause_t wakeup_reason= esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
    Serial.println("Waked up by BTN");
    checkLevelUntilLimit();
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER){
    Serial.println("Waked up by RTC");
    initSensors();
    measureSensors();
    sendPayloadAndWaitAck();
  } else {
    Serial.println("First Boot");
  }
  ledTurnOff();

  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}


void loop() { } // EMPTY


void configureWakeups() {
  // Configure wakeup via button (EXT0)
  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 1);
  rtc_gpio_pullup_dis(WAKEUP_GPIO);
  rtc_gpio_pulldown_en(WAKEUP_GPIO);

  // Configure wakeup source every "TIME_TO_SLEEP" seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.flush(); 
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
  esp_now_register_recv_cb(onAckRecv);
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


void onAckRecv(const esp_now_recv_info_t *mac, const uint8_t *data, int len) {
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
    // Wait callback to set ackReceived
    ledNewData();
    ledUpdateNewData();
    ledUpdateBlinking(); 
    delay(20);
  }
  
  if (ackReceived) {
    Serial.println("ACK OK recevied from master");
    ledAnimationBlinking(LED::green, 10);
  } else {
    Serial.println("ACK NOK (timeout)");
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


void checkLevelUntilLimit() {
  const int samples = 5;
  float distance_cm = 0;
  pinMode(US::echo, INPUT);
  pinMode(US::trig, OUTPUT);

  do{

    float sum = 0;

    for (int i = 0; i < samples; i++){
      digitalWrite(US::trig, LOW);
      delayMicroseconds(2);
      digitalWrite(US::trig, HIGH);
      delayMicroseconds(10);
      digitalWrite(US::trig, LOW);

      long duration = pulseIn(US::echo, HIGH, 30000); // timeout 30 ms
      float us_read = duration * 0.0343 / 2;

      sum += us_read;
      delay(50);
    }

    distance_cm = sum / samples;
    
    Serial.printf("Level: %.1f cm\n", distance_cm);
    ledAnimationSnake();
    delay(100);
  } while (distance_cm > US::limit);
  
  ledAnimationBlinking(LED::green, 20);
}