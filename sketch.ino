#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define MQ2_PIN 34
#define DS18B20_PIN 4

#define RELAY_FAN   26
#define RELAY_ALARM 27

#define LED_GREEN  13
#define LED_YELLOW 12
#define LED_RED    14

#define BUZZER 33

LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

const float BATAS_AMAN = 10.0;
const float BATAS_BAHAYA = 25.0;

float ppm = 0.0;
float persen = 0.0;
float suhu = 0.0;
int nilaiADC = 0;

unsigned long lastLCD = 0;
bool pageTemp = false;

void setup() {
  Serial.begin(115200);
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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("NH3 MONITORING");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(2000);
  lcd.clear();
}

void loop() {
  nilaiADC = analogRead(MQ2_PIN);

  // PEMETAAN SIMULASI, bukan kalibrasi NH3 nyata.
  ppm = (nilaiADC / 4095.0) * 100.0;
  persen = ppm * 0.0001;

  sensors.requestTemperatures();
  suhu = sensors.getTempCByIndex(0);

  if (ppm < BATAS_AMAN) {
    kondisiAman();
  } else if (ppm <= BATAS_BAHAYA) {
    kondisiWaspada();
  } else {
    kondisiBahaya();
  }

  if (millis() - lastLCD >= 2000) {
    lastLCD = millis();
    pageTemp = !pageTemp;
  }

  tampilLCD();

  Serial.print("ADC: ");
  Serial.print(nilaiADC);
  Serial.print(" | NH3: ");
  Serial.print(ppm, 2);
  Serial.print(" ppm | ");
  Serial.print(persen, 5);
  Serial.print(" % | Suhu: ");
  Serial.print(suhu, 2);
  Serial.println(" C");

  delay(200);
}

void kondisiAman() {
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);

  digitalWrite(RELAY_FAN, LOW);
  digitalWrite(RELAY_ALARM, LOW);

  noTone(BUZZER);
}

void kondisiWaspada() {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, HIGH);
  digitalWrite(LED_RED, LOW);

  digitalWrite(RELAY_FAN, HIGH);
  digitalWrite(RELAY_ALARM, LOW);

  noTone(BUZZER);
}

void kondisiBahaya() {
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, HIGH);

  digitalWrite(RELAY_FAN, HIGH);
  digitalWrite(RELAY_ALARM, HIGH);

  tone(BUZZER, 2000);
}

void tampilLCD() {
  lcd.clear();

  if (!pageTemp) {
    lcd.setCursor(0, 0);
    lcd.print("NH3:");
    lcd.print(ppm, 1);
    lcd.print(" ppm");

    lcd.setCursor(0, 1);

    if (ppm < BATAS_AMAN) {
      lcd.print("STATUS: AMAN");
    } else if (ppm <= BATAS_BAHAYA) {
      lcd.print("STATUS: WASPADA");
    } else {
      lcd.print("STATUS: BAHAYA");
    }
  } else {
    lcd.setCursor(0, 0);
    lcd.print("TEMP:");
    lcd.print(suhu, 1);
    lcd.print((char)223);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("NH3:");
    lcd.print(persen, 4);
    lcd.print("%");
  }
}
