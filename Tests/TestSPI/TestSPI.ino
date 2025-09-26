#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SD.h>

// Pinos do display
#define TFT_CS  15
#define TFT_DC  4
#define TFT_RST 2
#define TFT_LED 5

// Pinos do SD (VSPI)
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS   5
#define SD_FILE "/test.txt"

// Pinos do joystick
#define JOY_X_PIN 34
#define JOY_Y_PIN 35
#define JOY_BTN_PIN 26

// Menu
enum MenuState { MENU_MAIN };
int selectedOption = 0;
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

struct JoystickState {
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool click = false;
};

JoystickState readJoystick() {
  const int deadZone = 600;
  const int center = 2048;
  static int prevX = center, prevY = center;
  const float alpha = 0.7;

  int rawX = analogRead(JOY_X_PIN);
  int rawY = analogRead(JOY_Y_PIN);
  bool btn = digitalRead(JOY_BTN_PIN) == LOW;

  int x = alpha * rawX + (1 - alpha) * prevX;
  int y = alpha * rawY + (1 - alpha) * prevY;
  prevX = x;
  prevY = y;

  JoystickState js;
  if (x < center - deadZone) js.left = true;
  if (x > center + deadZone) js.right = true;
  if (y < center - deadZone) js.up = true;
  if (y > center + deadZone) js.down = true;
  js.click = btn;
  return js;
}

bool beginDisplayOperation() {
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);
  SPI.setFrequency(1000000);
  delay(10);
  digitalWrite(TFT_CS, LOW);
  return true;
}

void endDisplayOperation() {
  digitalWrite(TFT_CS, HIGH);
  SPI.end();
}

void drawMenu() {
  if (!beginDisplayOperation()) return;
  Serial.println("Before fillRect");
  tft.fillRect(0, 60, 160, 128, ST77XX_BLACK);
  Serial.println("After fillRect");
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 30);
  tft.print("--- MENU ---");
  const char* options[] = { "Option 1", "Option 2", "Option 3" };
  const int count = sizeof(options) / sizeof(options[0]);
  for (int i = 0; i < count; i++) {
    tft.fillRect(10, 60 + i * 20, 140, 16, i == selectedOption ? ST77XX_WHITE : ST77XX_BLACK);
    tft.setTextColor(i == selectedOption ? ST77XX_BLACK : ST77XX_WHITE, i == selectedOption ? ST77XX_WHITE : ST77XX_BLACK);
    tft.setCursor(10, 60 + i * 20);
    tft.print(options[i]);
  }
  Serial.println("Menu drawn successfully");
  endDisplayOperation();
}

void writeToSD() {
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  SPI.setFrequency(1000000);
  delay(200);
  digitalWrite(SD_CS, LOW);
  Serial.println("Initializing SD...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR init SD!");
    if (beginDisplayOperation()) {
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 60);
      tft.print("SD ERROR");
      endDisplayOperation();
    }
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  Serial.println("SD initialized successfully");
  File file = SD.open(SD_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("ERROR: file didn't open!");
    if (beginDisplayOperation()) {
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 60);
      tft.print("FILE ERROR");
      endDisplayOperation();
    }
    digitalWrite(SD_CS, HIGH);
    SPI.end();
    return;
  }
  file.println("Test write: " + String(millis()));
  file.close();
  Serial.println("Data written to SD");
  if (beginDisplayOperation()) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(10, 60);
    tft.print("SD OK");
    endDisplayOperation();
  }
  digitalWrite(SD_CS, HIGH);
  SPI.end();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_LED, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_LED, HIGH);
  delay(500);
  digitalWrite(TFT_CS, LOW);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, TFT_CS);
  SPI.setFrequency(1000000);
  Serial.println("Initializing display...");
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Display OK");
  Serial.println("Display initialized successfully");
  digitalWrite(TFT_CS, HIGH);
  SPI.end();
  drawMenu();
}

void loop() {
  static JoystickState prevJs;
  static unsigned long lastNavTime = 0;
  const unsigned long NAV_DEBOUNCE = 200;
  JoystickState js = readJoystick();
  unsigned long now = millis();
  if (now - lastNavTime > NAV_DEBOUNCE) {
    if (js.up && !prevJs.up) {
      selectedOption = (selectedOption + 2) % 3;
      drawMenu();
      lastNavTime = now;
    } else if (js.down && !prevJs.down) {
      selectedOption = (selectedOption + 1) % 3;
      drawMenu();
      lastNavTime = now;
    } else if (js.click && !prevJs.click) {
      writeToSD();
      delay(1000); // Mostrar mensagem no display
      drawMenu();
      lastNavTime = now;
    }
  }
  prevJs = js;
}