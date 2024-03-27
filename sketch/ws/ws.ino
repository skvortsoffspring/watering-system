#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

#define VERSION "Version: 0.0.2"

//#define DEBUG
#define LCD
//#define INIT

#ifdef LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

#define DRY 460
#define WET 185

#define SPEED_SERIAL 9600
#define OFFSET_ASCII 65

#define STAR '*'             // const for "Bluetooth Electronics"
#define PERIOD_ON_POMP 5000  // to config (time on pompa)

#define SIZE_AIO 4                 // size AIO Uno
#define OFFSET_FOR_SOLENOID_PIN 2  // skip RX, TX for bluetooth
char size_average = 0;             // to config (count measurements)

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
  for (IN in = A; in <= D; in = in + 1) {
    cs.settings[in].enable = 1;
    cs.settings[in].min_soil = 40;
    cs.settings[in].max_soil = 70;
  }
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

  resetAvg();

#ifdef LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Watering system");
  lcd.setCursor(0, 1);
  lcd.print(VERSION);
#endif
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
      IN in = convertToEnum(string[0]);
      if (A == in || B == in || C == in || D == in) {
        if (string[1] == '1') {
          cs.settings[in].enable = 1;
          averages[in] = 0;
        } else {
          cs.settings[in].enable = 0;
        }
        EEPROM.put(0, cs);
        EEPROM.get(0, cs);
      }
    }
  }

  if (millis() - prevMillis >= 1000) {

    for (IN in = A; in <= D; in = in + 1) {

      bool isEnable = cs.settings[in].enable;
      int value = 0;

      if (isEnable) {
        templateOn[1] = convertToChar(in) + 4;
        Serial.println(templateOn);
        averages[in] += value = map(analogRead(toAioPin(in)), DRY, WET, 0, 100);
      } else {
        templateOff[1] = convertToChar(in) + 4;
        Serial.println(templateOff);
      }

      counter_measuring++;

      formatAndSend(in, value);

#ifdef DEBUG
      Serial.print(convertToChar(in));
      Serial.println(analogRead(toAioPin(in)));
#endif

#ifdef LCD
      print(value, in, isEnable);
#endif

      if (counter_measuring >= cs.counter_of_size) {
        const int avg = averages[in] / counter_measuring;

        if (isEnable)
          check(avg, toSolenoidPin(in));

        if (D == in) {
          resetAvg();
          counter_measuring = 0;
        }
      }
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

inline int toAioPin(IN val) {
  return val + 14;
}

inline char convertToChar(IN pin) {
  return pin + OFFSET_ASCII;
}

inline IN convertToEnum(char ch) {
  return ch - OFFSET_ASCII;
}

#ifdef LCD
void print(const int value, const IN in, bool isEnable) {
  char length = 2;
  char buff[] = { convertToChar(in), '-', '-', 0x20, 0 };

  if (isEnable) {
    setConstrain(&value);
    length = strlen(itoa(value, buff + 1, 10)) + 1;
    memset(buff + length, 0x20, strlen(buff) + 1 - length);
  }
  lcd.setCursor(in * 4, 1);
  lcd.print(buff);
}
#endif

inline void setConstrain(int *pval) {
  if (*pval < 0)
    *pval = 0;
  if (*pval > 100)
    *pval = 100;
}

void formatAndSend(const IN pin, const int value) {
  char buff[] = { 0, 0, 0, 0, 0, 0 };  // for convert to "Bluetooth Electronics"
  buff[0] = STAR;
  buff[1] = convertToChar(pin);
  setConstrain(&value);
  buff[strlen(itoa(value, buff + 2, 10)) + 2] = STAR;
  Serial.println(buff);
}

void check(int val, int out) {
  if (val < 40) {
    digitalWrite(out, LOW);
    delay(PERIOD_ON_POMP);
    digitalWrite(out, HIGH);
  }
}