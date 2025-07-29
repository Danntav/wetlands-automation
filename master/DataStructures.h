#ifndef _DATA_STRUCTURES_H_
#define _DATA_STRUCTURES_H_

// Pinos do display
#define TFT_CS  15
#define TFT_RST 4
#define TFT_DC  2

// Pinos do SD (usando VSPI)
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS   5
#define SD_FILE "/data.csv"

// Pinos do joystick
#define JOY_X_PIN 32
#define JOY_Y_PIN 33
#define JOY_BTN_PIN 25

// RTC pins
#define RTC_SCL 22
#define RTC_SDA 21

// Constantes do projeto
#define TOTAL_SLAVES 9
#define MAX_ERRORS 10

// Estruturas
typedef struct struct_message {
  int id;
  float voltage;
  float temperature;
} struct_message;


typedef struct ack_message {
  int id;
  bool ok;
} ack_message;

struct JoystickState {
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool click = false;
};

#endif // DATA_STRUCTURE_H