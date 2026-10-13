#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ===================== PIN =====================
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

// Logika modul relay. Wokwi relay module = aktif-HIGH.
// Jika modul relay fisik Anda aktif-LOW, ubah menjadi LOW.
#define RELAY_ON_LEVEL  HIGH
#define RELAY_OFF_LEVEL (RELAY_ON_LEVEL == HIGH ? LOW : HIGH)

// ===================== OBJEK =====================
LiquidCrystal_I2C lcd(0x27, 20, 4);
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// ===================== AMBANG (NILAI SIMULASI 0-100, BUKAN ppm) =====================
const float BATAS_WASPADA = 4.0;   // < 4        : NORMAL
const float BATAS_BAHAYA  = 10.0;  // 4 .. 10    : WASPADA, > 10 : BAHAYA

enum Status { NORMAL = 0, WASPADA = 1, BAHAYA = 2 };

// ===================== PENGATURAN WAKTU =====================
const unsigned long GAS_INTERVAL_MS    = 100;   // baca gas
const unsigned long TEMP_CONVERT_MS    = 800;   // waktu konversi DS18B20 (non-blocking)
const unsigned long LCD_INTERVAL_MS    = 200;   // refresh LCD
const unsigned long SERIAL_INTERVAL_MS = 500;   // log serial
const unsigned long BEEP_INTERVAL_MS   = 250;   // bunyi/jeda saat WASPADA
const unsigned int  BUZZER_FREQ        = 2000;  // Hz

// ===================== VARIABEL =====================
float ch4 = 0, h2s = 0, nh3 = 0, suhu = DEVICE_DISCONNECTED_C;
bool  suhuError = true;

Status statusSekarang = NORMAL;
Status statusSebelum  = (Status)255;  // paksa penerapan output pertama kali

unsigned long tGas = 0, tLcd = 0, tSerial = 0, tTempRequest = 0, tBeep = 0;
bool tempRequested = false;
bool beepOn = false;

char lcdCache[4][21];  // cache isi LCD agar hanya menulis jika berubah

// ===================== FUNGSI BANTU =====================
float adcToSimValue(int adc) {
  return (adc / 4095.0f) * 100.0f;
}

Status klasifikasi(float v) {
  if (v > BATAS_BAHAYA)   return BAHAYA;
  if (v >= BATAS_WASPADA) return WASPADA;
  return NORMAL;
}

const char* statusText(Status s) {
  switch (s) {
    case BAHAYA:  return "BAHAYA";
    case WASPADA: return "WASPADA";
    default:      return "NORMAL";
  }
}

char statusHuruf(Status s) {
  switch (s) {
    case BAHAYA:  return 'B';
    case WASPADA: return 'W';
    default:      return 'N';
  }
}

// Tulis satu baris LCD (diisi spasi sampai 20 karakter) hanya jika berubah.
void lcdLine(uint8_t row, const char* text) {
  char buf[21];
  size_t len = strlen(text);
  if (len > 20) len = 20;
  memset(buf, ' ', 20);
  memcpy(buf, text, len);
  buf[20] = '\0';
  if (strcmp(buf, lcdCache[row]) != 0) {
    lcd.setCursor(0, row);
    lcd.print(buf);
    strcpy(lcdCache[row], buf);
  }
}

// Dipanggil HANYA saat status berubah: LED, relay, dan buzzer awal.
void terapkanStatus(Status s) {
  digitalWrite(LED_GREEN,  s == NORMAL  ? HIGH : LOW);
  digitalWrite(LED_YELLOW, s == WASPADA ? HIGH : LOW);
  digitalWrite(LED_RED,    s == BAHAYA  ? HIGH : LOW);

  digitalWrite(RELAY_FAN,   (s == WASPADA || s == BAHAYA) ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
  digitalWrite(RELAY_ALARM, (s == BAHAYA) ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);

  // Buzzer: satu-satunya tempat tone()/noTone() untuk NORMAL dan BAHAYA
  if (s == BAHAYA) {
    tone(BUZZER, BUZZER_FREQ);       // terus-menerus, tanpa batas waktu
    beepOn = true;
  } else if (s == WASPADA) {
    tone(BUZZER, BUZZER_FREQ);       // mulai dengan bunyi
    beepOn = true;
    tBeep = millis();
  } else {
    noTone(BUZZER);
    beepOn = false;
  }
}

// Pola putus-putus saat WASPADA (non-blocking). Status lain: tidak melakukan apa-apa.
void updateBuzzerWaspada() {
  if (statusSekarang != WASPADA) return;
  if (millis() - tBeep >= BEEP_INTERVAL_MS) {
    tBeep = millis();
    beepOn = !beepOn;
    if (beepOn) tone(BUZZER, BUZZER_FREQ);
    else        noTone(BUZZER);
  }
}

// Pembacaan DS18B20 non-blocking
void updateSuhu() {
  if (!tempRequested) {
    sensors.requestTemperatures();
    tTempRequest = millis();
    tempRequested = true;
  } else if (millis() - tTempRequest >= TEMP_CONVERT_MS) {
    float t = sensors.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C || t < -100.0f) {
      suhuError = true;
    } else {
      suhuError = false;
      suhu = t;
    }
    tempRequested = false;
  }
}

void updateLcd(float maxGas) {
  char line[32];
  Status sNH3 = klasifikasi(nh3);
  Status sCH4 = klasifikasi(ch4);
  Status sH2S = klasifikasi(h2s);

  snprintf(line, sizeof(line), "NH3:%5.1f %s", nh3, statusText(sNH3));
  lcdLine(0, line);

  snprintf(line, sizeof(line), "CH4:%3.0f%c H2S:%3.0f%c",
           ch4, statusHuruf(sCH4), h2s, statusHuruf(sH2S));
  lcdLine(1, line);

  if (suhuError) snprintf(line, sizeof(line), "SUHU: ERROR");
  else           snprintf(line, sizeof(line), "SUHU: %.1f%cC", suhu, (char)223);
  lcdLine(2, line);

  snprintf(line, sizeof(line), "STATUS: %s", statusText(klasifikasi(maxGas)));
  lcdLine(3, line);
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  Wire.begin(21, 22);

  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_ALARM, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY_FAN, RELAY_OFF_LEVEL);
  digitalWrite(RELAY_ALARM, RELAY_OFF_LEVEL);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  noTone(BUZZER);

  lcd.init();
  lcd.backlight();

  sensors.begin();
  sensors.setWaitForConversion(false);  // agar loop tidak terblokir

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MONITOR KUALITAS");
  lcd.setCursor(0, 1);
  lcd.print("DAGING IoT START");
  lcd.setCursor(0, 3);
  lcd.print("Memulai sistem...");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("MEMBACA SENSOR...");
  delay(800);
  lcd.clear();

  for (int i = 0; i < 4; i++) lcdCache[i][0] = '\0';
  // Isi cache dengan string yang pasti berbeda agar baris pertama selalu ditulis
  for (int i = 0; i < 4; i++) { memset(lcdCache[i], '#', 20); lcdCache[i][20] = '\0'; }
}

// ===================== LOOP =====================
void loop() {
  unsigned long now = millis();

  // --- Baca gas berkala ---
  if (now - tGas >= GAS_INTERVAL_MS) {
    tGas = now;
    ch4 = adcToSimValue(analogRead(CH4_PIN));
    h2s = adcToSimValue(analogRead(H2S_PIN));
    nh3 = adcToSimValue(analogRead(NH3_PIN));

    float maxGas = max(ch4, max(h2s, nh3));
    statusSekarang = klasifikasi(maxGas);

    // Output hanya diatur ulang jika status BERUBAH
    if (statusSekarang != statusSebelum) {
      terapkanStatus(statusSekarang);
      statusSebelum = statusSekarang;
    }
  }

  // --- Suhu (non-blocking) ---
  updateSuhu();

  // --- Pola buzzer WASPADA ---
  updateBuzzerWaspada();

  float maxGas = max(ch4, max(h2s, nh3));

  // --- LCD ---
  if (now - tLcd >= LCD_INTERVAL_MS) {
    tLcd = now;
    updateLcd(maxGas);
  }

  // --- Serial ---
  if (now - tSerial >= SERIAL_INTERVAL_MS) {
    tSerial = now;
    Serial.print("CH4(sim): "); Serial.print(ch4, 1);
    Serial.print(" | H2S(sim): "); Serial.print(h2s, 1);
    Serial.print(" | NH3(sim): "); Serial.print(nh3, 1);
    Serial.print(" | Suhu: ");
    if (suhuError) Serial.print("ERROR");
    else           Serial.print(suhu, 1);
    Serial.print(" C | Status: "); Serial.println(statusText(statusSekarang));
  }
}
