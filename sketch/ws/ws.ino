#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <limits.h>

#define VERSION "Version: 0.0.3"
#define START_ADDRESS 0

//#define DEBUG
#define LCD
#define INIT

#ifdef LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

#define DRY 460
#define WET 185

#define SPEED_SERIAL 9600
#define OFFSET_ASCII 65

#define STAR '*'  // const for "Bluetooth Electronics"

#define SIZE_AIO 4                 // size AIO Uno
#define OFFSET_FOR_SOLENOID_PIN 2  // skip RX, TX for bluetooth

#define MIN_VALUE 0
#define MAX_VALUE 100

char size_average = 0;  // to config (count measurements)

char templateOn[] = "*_R255G255B0*";
char templateOff[] = "*_R0G0B0*";

namespace IN {
enum IN { A,
          B,
          C,
          D };
}

namespace RGB {
enum RGB : char { R = 'R',
                  G = 'G',
                  B = 'B' };
}

struct HumiditySettings {
  unsigned int counter;
  unsigned int accum;
  bool enable;
  unsigned char minSoil;
  unsigned char maxSoil;
};

struct CommonSettings {
  unsigned char counterSize;
  unsigned char timeOnPomp;
  unsigned char maxAttempt;
  HumiditySettings settings[SIZE_AIO];
};

CommonSettings cs;
unsigned long prevMillis = millis();

void init_eeprom() {
  cs.counterSize = 60;
  cs.timeOnPomp = 5;
  cs.maxAttempt = 20;
  for (IN::IN in = IN::A; in <= IN::D; in = in + 1) {
    cs.settings[in].enable = 1;
    cs.settings[in].counter = 0;
    cs.settings[in].minSoil = 40;
    cs.settings[in].maxSoil = 70;
    cs.settings[in].accum = 0;
  }
  EEPROM.put(START_ADDRESS, cs);
}

void setup() {

#ifdef INIT
  init_eeprom();
#endif

  Serial.begin(SPEED_SERIAL);
  EEPROM.get(START_ADDRESS, cs);

  for (IN::IN i = IN::A; i <= IN::D; i = i + 1) {
    pinMode(toSolenoidPin(i), OUTPUT);
    digitalWrite(toSolenoidPin(i), HIGH);
  }


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
    Serial.flush();
    memset(string, 0, 8);
    for (int i = 0; i < availableBytes; i++) {
      string[i] = Serial.read();
    }

    if (2 == availableBytes) {
      IN::IN in = convertToInEnum(string[0]);
      if (IN::A == in || IN::B == in || IN::C == in || IN::D == in) {
        if (string[1] == '1') {
          cs.settings[in].enable = true;
          cs.settings[in].accum = 0;
        } else {
          cs.settings[in].enable = false;
          cs.settings[in].accum = 0;
        }
        EEPROM.put(START_ADDRESS, cs);
        EEPROM.get(START_ADDRESS, cs);
      }
    }
  }

  if (millis() - prevMillis >= 1000) {

    for (IN::IN in = IN::A; in <= IN::D; in = in + 1) {
      int value = 0;
      if (cs.settings[in].enable) {
        cs.settings[in].counter++;
        value = map(analogRead(toAioPin(in)), DRY, WET, 0, 100);
        setConstrain(&value);
        cs.settings[in].accum += value;
        templateOn[1] = convertToChar(in) + 4;
        Serial.println(templateOn);
      } else {
        templateOff[1] = convertToChar(in) + 4;
        Serial.println(templateOff);
      }

      formatAndSend(in, value);

#ifdef LCD
      print(value, in, cs.settings[in].enable);
#endif

      prevMillis = millis();

      if (cs.settings[in].enable && cs.counterSize == cs.settings[in].counter) {
        check(cs.settings[in].accum / cs.settings[in].counter, in);
        resetMeasuring(in);
      }
    }
  }
}

void resetMeasuring(IN::IN in) {
  cs.settings[in].accum = 0;
  cs.settings[in].counter = 0;
}

inline int toSolenoidPin(IN::IN val) {
  return val + OFFSET_FOR_SOLENOID_PIN;
}

inline int toAioPin(IN::IN val) {
  return val + 14;
}

inline char convertToChar(IN::IN pin) {
  return pin + OFFSET_ASCII;
}

inline IN::IN convertToInEnum(char ch) {
  return ch - OFFSET_ASCII;
}

#ifdef LCD
void print(const int value, const IN::IN in, bool isEnable) {
  char length = 2;
  char buff[] = { convertToChar(in), '-', '-', 0x20, 0 };

  if (isEnable) {
    length = strlen(itoa(value, buff + 1, 10)) + 1;
    memset(buff + length, 0x20, strlen(buff) + 1 - length);
  }
  lcd.setCursor(in * 4, 1);
  lcd.print(buff);
}
#endif

inline void setConstrain(int *pval) {
  if (*pval < MIN_VALUE)
    *pval = MIN_VALUE;
  if (*pval > MAX_VALUE)
    *pval = MAX_VALUE;
}

void formatAndSend(const IN::IN pin, const int value) {
  const char size = 6;
  const char offset = 2;
  char out[] = { 0, 0, 0, 0, 0, 0 };
  char buff[3];  // 0-100
  out[0] = STAR;
  out[1] = convertToChar(pin);
  setConstrain(&value);
  itoa(value, buff, 10);
  strcat(out, buff);
  out[strlen(out)] = STAR;
  Serial.println(out);
}

void check(int val, IN::IN in) {
  const int pin = toSolenoidPin(in);
#ifdef DEBUG
  Serial.print("check: ");
  Serial.print(convertToChar(in));
  Serial.println(val);
#endif
  if (val < cs.settings[in].minSoil) {
    digitalWrite(pin, LOW);
    delay(cs.timeOnPomp * 1000);
    digitalWrite(pin, HIGH);
  }
}