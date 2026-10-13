#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define CH4_PIN       34
#define H2S_PIN       35
#define NH3_PIN       32
#define DS18B20_PIN    4

#define RELAY_FAN     26
#define RELAY_ALARM   27
#define LED_GREEN     13
#define LED_YELLOW    12
#define LED_RED       14
#define BUZZER        33

LiquidCrystal_I2C lcd(0x27, 20, 4);
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

const float BATAS_WASPADA = 4.0;
const float BATAS_BAHAYA  = 25.0;

float ch4 = 0, h2s = 0, nh3 = 0, suhu = 0;
unsigned long alarmStarted = 0;
const unsigned long ALARM_DURATION = 7000;
bool dangerPrevious = false;
bool alarmActive = false;

float adcToSimValue(int adc) {
  return (adc / 4095.0f) * 100.0f;
}

const char* statusText(float value) {
  if (value < BATAS_WASPADA) return "AMAN";
  if (value <= BATAS_BAHAYA) return "WASPADA";
  return "BAHAYA";
}


unsigned long waktuBuzzer = 0;
bool statusBuzzer = false;

void setStatus(float value) {
  bool danger = value > BATAS_BAHAYA;
  bool warning = value >= BATAS_WASPADA &&
                 value <= BATAS_BAHAYA;

  digitalWrite(LED_GREEN, (!warning && !danger) ? HIGH : LOW);
  digitalWrite(LED_YELLOW, warning ? HIGH : LOW);
  digitalWrite(LED_RED, danger ? HIGH : LOW);

  digitalWrite(RELAY_FAN, (warning || danger) ? HIGH : LOW);
  digitalWrite(RELAY_ALARM, danger ? HIGH : LOW);

  if (danger) {
    tone(BUZZER, 2000);
  }

  else if (warning) {
    if (millis() - waktuBuzzer >= 250) {
      waktuBuzzer = millis();
      statusBuzzer = !statusBuzzer;

      if (statusBuzzer) {
        tone(BUZZER, 2000);
      } else {
        noTone(BUZZER);
      }
    }
  }

  else {
    noTone(BUZZER);
    statusBuzzer = false;
    waktuBuzzer = millis();
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  sensors.begin();

  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_ALARM, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY_FAN, LOW);
  digitalWrite(RELAY_ALARM, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  noTone(BUZZER);

  // Layar pembuka
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MONITOR KUALITAS");
  lcd.setCursor(0, 1);
  lcd.print("DAGING IoT START");
  lcd.setCursor(0, 3);
  lcd.print("Memulai sistem...");
  delay(2500);

  // Layar inisialisasi
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("MEMBACA SENSOR...");
  delay(1000);
  lcd.clear();
}

void loop() {
  ch4 = adcToSimValue(analogRead(CH4_PIN));
  h2s = adcToSimValue(analogRead(H2S_PIN));
  nh3 = adcToSimValue(analogRead(NH3_PIN));

  sensors.requestTemperatures();
  suhu = sensors.getTempCByIndex(0);

  float maxGas = max(ch4, max(h2s, nh3));
  setStatus(maxGas);

  if (alarmActive && millis() - alarmStarted >= ALARM_DURATION) {
    noTone(BUZZER);
    alarmActive = false;
  }

  // Empat baris LCD 20x4: NH3 ditampilkan lebih dahulu,
  // lalu gas lain, suhu, dan status keseluruhan.
  lcd.setCursor(0, 0);
  lcd.print("NH3: ");
  lcd.print(nh3, 1);
  lcd.print(" ");
  lcd.print(statusText(nh3));
  lcd.print("       ");

  lcd.setCursor(0, 1);
  lcd.print("CH4:");
  lcd.print(ch4, 0);
  lcd.print(" H2S:");
  lcd.print(h2s, 0);
  lcd.print("       ");

  lcd.setCursor(0, 2);
  lcd.print("SUHU DAGING: ");
  if (suhu == DEVICE_DISCONNECTED_C) {
    lcd.print("ERROR   ");
  } else {
    lcd.print(suhu, 1);
    lcd.write((uint8_t)223);
    lcd.print("C   ");
  }

  lcd.setCursor(0, 3);
  lcd.print("STATUS: ");
  lcd.print(statusText(maxGas));
  lcd.print("           ");

  Serial.print("CH4(sim): "); Serial.print(ch4, 1);
  Serial.print(" | H2S(sim): "); Serial.print(h2s, 1);
  Serial.print(" | NH3(sim): "); Serial.print(nh3, 1);
  Serial.print(" | Suhu: "); Serial.print(suhu, 1);
  Serial.print(" C | Status: "); Serial.println(statusText(maxGas));

  delay(500);
}
