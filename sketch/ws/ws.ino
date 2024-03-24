#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define VERSION "Version: 0.0.2"

LiquidCrystal_I2C lcd(0x27, 16, 2);

//#define DEBUG
#define LCD
//#define INIT

#define DRY 460
#define WET 190

#define SPEED_SERIAL 9600
#define OFFSET_ASCII 65

#define STAR '*'              // const for "Bluetooth Electronics"
#define PERIOD_ON_POMP 5000  // to config (time on pompa)

#define SIZE_AIO 4                 // size AIO Uno
#define OFFSET_FOR_SOLENOID_PIN 2  // skip RX, TX for bluetooth
char size_average = -1;            // to config (count measurements)

int counter_measuring = 0;  // depends COUNT_AVERAGE
int averages[SIZE_AIO];
char templateOn[] = "*_R255G255B0*";
char templateOff[] = "*_R0G0B0*";

enum IN { A,
          B,
          C,
          D };

struct HumiditySettings {
  bool enable;
  char min_soil;
  char max_soil;
};

struct CommonSettings {
  short counter_of_size;
  char max_attempt;
  HumiditySettings settings[SIZE_AIO];
};

CommonSettings cs;
unsigned long prevMillis = millis();

void init_eeprom() {
  cs.counter_of_size = 60;
  cs.max_attempt = 20;
  cs.settings[0].enable = 1;
  cs.settings[0].min_soil = 40;
  cs.settings[0].max_soil = 70;

  cs.settings[1].enable = 1;
  cs.settings[1].min_soil = 40;
  cs.settings[1].max_soil = 70;

  cs.settings[2].enable = 1;
  cs.settings[2].min_soil = 40;
  cs.settings[2].max_soil = 70;

  cs.settings[3].enable = 1;
  cs.settings[3].min_soil = 40;
  cs.settings[3].max_soil = 70;

  EEPROM.put(0, cs);
}

void setup() {

#ifdef INIT
  init_eeprom();
#endif

  Serial.begin(SPEED_SERIAL);
  EEPROM.get(0, cs);

  Serial.print("Size averages: ");
  Serial.println((int)cs.counter_of_size);

  pinMode(toSolenoidPin(A), OUTPUT);
  pinMode(toSolenoidPin(B), OUTPUT);
  pinMode(toSolenoidPin(C), OUTPUT);
  pinMode(toSolenoidPin(D), OUTPUT);

  digitalWrite(toSolenoidPin(A), HIGH);
  digitalWrite(toSolenoidPin(B), HIGH);
  digitalWrite(toSolenoidPin(C), HIGH);
  digitalWrite(toSolenoidPin(D), HIGH);

  resetAvg();  // inline

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Watering system");
  lcd.setCursor(0, 1);
  lcd.print(VERSION);
}

void loop() {
  int availableBytes = Serial.available();
  char string[8] = { 0 };

  if (availableBytes == 2) {
    memset(string, 0, 8);
    for (int i = 0; i < availableBytes; i++) {
      string[i] = Serial.read();
    }

    if (2 == availableBytes) {
      IN res = convertToEnum(string[0]);
      if (A == res || B == res || C == res || D == res) {
        if (string[1] == '1') {
          cs.settings[res].enable = 1;
        } else {
          cs.settings[res].enable = 0;
        }
        EEPROM.put(0, cs);
        EEPROM.get(0, cs);
      }
    }
  }

  if (millis() - prevMillis >= 1000) {

    for (int i = 0; i < 4; i++) {
      if (cs.settings[i].enable) {
        templateOn[1] = convertToChar((IN)i) + 4;
        Serial.println(templateOn);
      } else {
        templateOff[1] = convertToChar((IN)i) + 4;
        Serial.println(templateOff);
      }
    }

    averages[A] += map(analogRead(A0), DRY, WET, 0, 100);
    averages[B] += map(analogRead(A1), DRY, WET, 0, 100);
    averages[C] += map(analogRead(A2), DRY, WET, 0, 100);
    averages[D] += map(analogRead(A3), DRY, WET, 0, 100);

    counter_measuring++;

    formatAndSend(A);
    formatAndSend(B);
    formatAndSend(C);
    formatAndSend(D);

#ifdef DEBUG
    Serial.print("A0 rawData: ");
    Serial.println(analogRead(A0));

    Serial.print("A1 rawData: ");
    Serial.println(analogRead(A1));

    Serial.print("A2 rawData: ");
    Serial.println(analogRead(A2));

    Serial.print("A3 rawData: ");
    Serial.println(analogRead(A3));
#endif

    if (cs.counter_of_size == counter_measuring) {
      const int avg_0 = averages[A] / cs.counter_of_size;
      const int avg_1 = averages[B] / cs.counter_of_size;
      const int avg_2 = averages[C] / cs.counter_of_size;
      const int avg_3 = averages[D] / cs.counter_of_size;

      print(0, 1, avg_0, A);   // TODO improve
      print(4, 1, avg_1, B);   //
      print(8, 1, avg_2, C);   //
      print(12, 1, avg_3, D);  //

      check(avg_0, toSolenoidPin(A));  // for  testing using now only A and D
      //check(avg_1, toSolenoidPin(B));
      //check(avg_2, toSolenoidPin(C));
      check(avg_3, toSolenoidPin(D));

      resetAvg();
      counter_measuring = 0;
    }
    prevMillis = millis();
  }
}

inline void resetAvg() {
  memset(averages, 0, sizeof(int) * SIZE_AIO);
}

inline int toSolenoidPin(IN val) {
  return val + OFFSET_FOR_SOLENOID_PIN;
}

inline char convertToChar(IN pin) {
  return pin + OFFSET_ASCII;
}

inline IN convertToEnum(char ch) {
  return ch - OFFSET_ASCII;
}

void print(const int lcdL, const int lcdC, const int val, const IN pin) {
  const char size = 5;
  char buff[size];
  buff[0] = convertToChar(pin);
  setConstrain(&val);
  char length = strlen(itoa(val, buff + 1, 10)) + 1;
  memset(buff + length, 0x20, size - length);
  lcd.setCursor(lcdL, lcdC);
  lcd.print(buff);
}

inline void setConstrain(int *pval) {
  if (*pval < 0)
    *pval = 0;
  if (*pval > 99)
    *pval = 99;
}

void formatAndSend(const IN pin) {
  char buff[] = { 0, 0, 0, 0, 0, 0 };  // for convert to "Bluetooth Electronics"
  const int avg = averages[pin] / counter_measuring;
  buff[0] = STAR;
  buff[1] = convertToChar(pin);
  buff[strlen(itoa(avg, buff + 2, 10)) + 2] = STAR;
  Serial.println(buff);
}

void check(int val, int out) {
  if (val < 40) {
    digitalWrite(out, LOW);
    delay(PERIOD_ON_POMP);
    digitalWrite(out, HIGH);
  }
}