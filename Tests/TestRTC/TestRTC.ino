#include <Wire.h>
#include <RTClib.h>

#define RTC_SDA 21
#define RTC_SCL 22

RTC_DS1307 rtc;

void setup() {
  Serial.begin(115200);
  delay(2000); // Aguardar inicialização do serial

  Serial.println("Iniciando ajuste do RTC...");
  
  // Inicializar I2C com os pinos específicos
  Wire.begin(RTC_SDA, RTC_SCL);
  delay(100);

  // Verificar se o RTC está conectado
  if (!rtc.begin()) {
    Serial.println("ERRO CRÍTICO: RTC não encontrado! Verifique a fiação.");
    Serial.println("Verificando dispositivos I2C...");
    scanI2C();
    while(1); // Trava aqui se não encontrar o RTC
  }

  Serial.println("RTC encontrado!");

  // Forçar o ajuste do RTC (descomente apenas quando quiser ajustar)
  // IMPORTANTE: Comente esta linha após o primeiro ajuste para evitar reajustes desnecessários
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("RTC ajustado para a hora de compilação.");
  
  // Alternativa: ajustar manualmente para uma data/hora específica
  // rtc.adjust(DateTime(2025, 9, 27, 14, 30, 0)); // Ano, Mês, Dia, Hora, Minuto, Segundo
  
  Serial.println("------------------------------------");
  Serial.println("RTC ajustado com sucesso!");
  Serial.println("A hora atual é:");
  Serial.println("------------------------------------");
}

void loop() {
  // Verificar se o RTC está funcionando
  if (!rtc.isrunning()) {
    Serial.println("ERRO: RTC parou de funcionar!");
    delay(5000);
    return;
  }

  DateTime now = rtc.now();
  
  // Usar formatação manual ao invés de toString() para evitar problemas de memória
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", 
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
  
  delay(2000);
}

void scanI2C() {
  Serial.println("Escaneando barramento I2C...");
  int dispositivos = 0;
  
  for (byte endereco = 1; endereco < 127; endereco++) {
    Wire.beginTransmission(endereco);
    byte erro = Wire.endTransmission();
    
    if (erro == 0) {
      Serial.printf("Dispositivo encontrado no endereço 0x%02X\n", endereco);
      dispositivos++;
    }
  }
  
  if (dispositivos == 0) {
    Serial.println("Nenhum dispositivo I2C encontrado!");
    Serial.println("Verifique as conexões:");
    Serial.println("- VCC do RTC -> 5V ou 3.3V");
    Serial.println("- GND do RTC -> GND");
    Serial.println("- SDA do RTC -> Pino 21");
    Serial.println("- SCL do RTC -> Pino 22");
  } else {
    Serial.printf("Total de dispositivos encontrados: %d\n", dispositivos);
  }
}