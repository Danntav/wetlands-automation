#ifndef _DATA_STRUCTURES_H_
#define _DATA_STRUCTURES_H_

// Display pins
#define TFT_CS  15
#define TFT_RST 2
#define TFT_DC  4

// SD pins
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS   5
#define SD_FILE "/data.csv"

// Joystick pins
#define JOY_X_PIN 34
#define JOY_Y_PIN 35
#define JOY_BTN_PIN 26

// RTC pins
#define RTC_SCL 22
#define RTC_SDA 21

// Constants
#define TOTAL_SLAVES 9
#define MAX_ERRORS 10

// Structs
typedef struct struct_message {
  int id;
  float voltage;
  float temperature;
} struct_message;


typedef struct ack_message {
  int id;
  bool ok;
} ack_message;

typedef struct struct_request {
  int command; //  1 para "coletar e enviar dados"
} struct_request;

struct JoystickState {
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool click = false;
};

#endif // DATA_STRUCTURE_H