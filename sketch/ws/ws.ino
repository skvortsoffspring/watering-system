#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <limits.h>

#define VERSION "*VVersion: 0.1.0"
#define START_ADDRESS_EEPROM 0

//#define DEBUG
#define LCD
//#define INIT

#define BEEPER 13

#ifdef LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
#endif

#define SPEED_SERIAL 9600
#define OFFSET_ASCII 65
#define OFFSET_ASCII_L 97

#define STAR '*'           // const for "Bluetooth Electronics"
#define COMAND_SYMBOL '#'  // start/stop commad bits

#define SIZE_AIO 4                 // size AIO Uno
#define OFFSET_FOR_SOLENOID_PIN 2  // skip RX, TX for bluetooth
#define MAX_LENGTH_COMMAND 8       // lenth command

#define MIN_VALUE 0
#define MAX_VALUE 99
#define MAX_ATTEMPT 5

#define COUNTER_SIZE 60       // times
#define PERIOD_MEASAURE 1000  // ms

char templateOn[] = "*_R255G255B0*";  // TODO try avoid const
char templateOff[] = "*_R0G0B0*";

namespace IN {
enum IN { A,
          B,
          C,
          D
};
}

enum RGB : char { R = 'R',
                  G = 'G',
                  B = 'B'
};



enum COMMAND : char { ENABLED = 'E',
                      MIN_SOIL = 'N',
                      MAX_SOIL = 'X',
                      TIME_WATERING = 'T',
                      GET_SETTINGS = 'G',
                      DRY = 'D',
                      WET = 'W',
};

struct Measurement {
  unsigned short counter;
  unsigned short accum;
  unsigned long startWatering;
  unsigned char attempt;
  Measurement()
    : attempt(0) {}
};

struct HumiditySettings {
  bool enabled;
  unsigned char minSoil;
  unsigned char maxSoil;
  unsigned char timeOnPomp;
  unsigned short dry;
  unsigned short wet;
} hs;

/* GLOBAL VARIABLES */
unsigned long prevMillis = millis();
Measurement measurement[SIZE_AIO];
char command[MAX_LENGTH_COMMAND];
char positionCommand = 0;
bool emergencyMode = false;

#ifdef INIT
void init_eeprom() {
  HumiditySettings settings[SIZE_AIO];
  for (IN::IN in = IN::A; in <= IN::D; in = in + 1) {
    settings[in].enabled = 1;
    settings[in].minSoil = 40;
    settings[in].maxSoil = 70;
    settings[in].timeOnPomp = 5;
    settings[in].dry = 640;
    settings[in].wet = 320;
  }
  EEPROM.put(START_ADDRESS_EEPROM, settings);
}
#endif

void setup() {

#ifdef INIT
  init_eeprom();
#endif

  Serial.begin(SPEED_SERIAL);

  for (IN::IN i = IN::A; i <= IN::D; i = i + 1) {
    pinMode(toSolenoidPin(i), OUTPUT);
    digitalWrite(toSolenoidPin(i), HIGH);
  }

  pinMode(BEEPER, OUTPUT);
  memset(command, 0, MAX_LENGTH_COMMAND);

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
  if (!emergencyMode) {
    int availableBytes = Serial.available();

    if (availableBytes) {
      Serial.flush();
      char symbol = Serial.read();
      if (positionCommand == 0 && symbol == COMAND_SYMBOL) {
        command[positionCommand++] = symbol;
      } else if (positionCommand != 0 && symbol != COMAND_SYMBOL) {
        command[positionCommand++] = symbol;
      } else if (positionCommand != 0 && symbol == COMAND_SYMBOL) {
        processCommand();
        memset(command, 0, MAX_LENGTH_COMMAND);
        positionCommand = 0;
      }
    }

    if (millis() - prevMillis >= PERIOD_MEASAURE) {

      for (IN::IN in = IN::A; in <= IN::D; in = in + 1) {
        int value = 0;
        EEPROM.get(START_ADDRESS_EEPROM + sizeof(HumiditySettings) * in, hs);

        if (hs.enabled) {
          measurement[in].counter++;
          value = map(analogRead(toAioPin(in)), hs.dry, hs.wet, MIN_VALUE, MAX_VALUE);
          setConstrain(&value);
          measurement[in].accum += value;
          templateOn[1] = convertToChar(in) + 4;
          Serial.println(templateOn);
        } else {
          templateOff[1] = convertToChar(in) + 4;
          Serial.println(templateOff);
        }
        Serial.println(VERSION);

        formatAndSend(in, value);

#ifdef LCD
        print(value, in, hs.enabled);
#endif

        prevMillis = millis();

        if (hs.enabled && (COUNTER_SIZE == measurement[in].counter)) {
          check(measurement[in].accum / measurement[in].counter, in);
          measurement[in].accum = 0;
          measurement[in].counter = 0;
        }

        if (0 != measurement[in].startWatering && measurement[in].startWatering + hs.timeOnPomp * 1000 < millis()) {
          digitalWrite(toSolenoidPin(in), HIGH);
          measurement[in].startWatering = 0;
        }
      }
    }
  } else {
    emergency();
  }
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

inline void setConstrain(int *pval) {
  if (*pval < MIN_VALUE)
    *pval = MIN_VALUE;
  if (*pval > MAX_VALUE)
    *pval = MAX_VALUE;
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


void formatAndSend(const IN::IN pin, const int value) {
  const char size = 6;
  const char offset = 2;
  char out[] = { 0, 0, 0, 0, 0, 0 };
  char buff[3];  // 0-100
  out[0] = STAR;
  out[1] = convertToChar(pin);
  itoa(value, buff, 10);
  strcat(out, buff);
  out[strlen(out)] = STAR;
  Serial.println(out);
}

void check(int val, IN::IN in) {
#ifndef DEBUG
  if (measurement[in].attempt >= MAX_ATTEMPT) {
    emergencyMode = true;
  }
#endif
  if (val < hs.minSoil && 0 == measurement[in].startWatering) {
    digitalWrite(toSolenoidPin(in), LOW);
    measurement[in].startWatering = millis();
    measurement[in].attempt++;
  }

  if (val >= hs.maxSoil) {
    measurement[in].attempt = 0;
  }
}

void processCommand() {
  IN::IN in = convertToInEnum(command[1]);
  if (IN::A == in || IN::B == in || IN::C == in || IN::D == in) {
    EEPROM.get(START_ADDRESS_EEPROM + sizeof(HumiditySettings) * in, hs);
#ifdef DEBUG
    Serial.print("#command: ");
    Serial.println(command);
#endif

    switch (command[2]) {
      case COMMAND::ENABLED:
        {
          bool isEnabled = (command[3] == '1');
          if (hs.enabled != isEnabled) {
            hs.enabled = isEnabled;
          } else {
            return;
          }
          break;
        }
      case COMMAND::MIN_SOIL:
        {
          int min = extractCommandValue();
          if (hs.minSoil == min) {
            return;
          } else if (min < hs.maxSoil) {
            hs.minSoil = min;
          } else {
            err("Value min soil must be less than max soil");
            return;
          }
          break;
        }
      case COMMAND::MAX_SOIL:
        {
          int max = extractCommandValue();
          if (hs.maxSoil == max) {
            return;
          } else if (max > hs.minSoil) {
            hs.maxSoil = max;
          } else {
            err("Value max soil must be more than min soil");
            return;
          }
          break;
        }
      case COMMAND::TIME_WATERING:
        {
          int range = extractCommandValue();
          if (hs.timeOnPomp == range) {
            return;
          } else if (range > 0 && range < 20) {
            hs.timeOnPomp = range;
          } else {
            err("Watering time must be 1 - 19 s");
            return;
          }
          break;
        }
      case COMMAND::DRY:
        {
          int range = extractCommandValue();
          if (hs.dry == range) {
            return;
          } else if (range > 0 && range < 1024) {
            hs.dry = range;
          } else {
            err("Dry value must be 1 - 1023");
            return;
          }
          break;
        }
      case COMMAND::WET:
        {
          int range = extractCommandValue();
          if (hs.wet == range) {
            return;
          } else if (range > 0 && range < 1024) {
            hs.wet = range;
          } else {
            err("Wet value must be 1 - 1023");
            return;
          }
          break;
        }
      case COMMAND::GET_SETTINGS:
        {
          sendSettings(in);
          return;
        }
      default:
        {
          err("Unrecognized command");
          return;
        }
    }

    EEPROM.put(START_ADDRESS_EEPROM + sizeof(HumiditySettings) * in, hs);
    sendSettings(in);
  }
}

int extractCommandValue() {
  char buffer[4] = { 0, 0, 0, 0 };
  char i = 0;
  while (command[i + 3]) {
    buffer[i] = command[i + 3];
    i++;
  }
#ifdef DEBUG
  Serial.print("int: ");
  Serial.println(atoi(buffer));
#endif
  return atoi(buffer);
}

void err(const char *err) {
  char buffer[64];
  sprintf(buffer, "*@ERR: %s", err);
  Serial.println(buffer);
}

void emergency() {
  for (IN::IN in = IN::A; in <= IN::D; in = in + 1) {
    digitalWrite(toSolenoidPin(in), HIGH);
  }

#ifdef LCD
  const char err[] = "Emergency mode  ";
  lcd.setCursor(0, 1);
  lcd.print(err);
#endif

  while (true) {
    for (char i = 0; i < 3; i++) {
      digitalWrite(BEEPER, HIGH);
      delay(100);
      digitalWrite(BEEPER, LOW);
      delay(100);
    }
    delay(60 * 60 * 1000);
  }
}

void sendSettings(IN::IN in) {
  EEPROM.get(START_ADDRESS_EEPROM + sizeof(HumiditySettings) * in, hs);
  char buffer[64];

  sprintf(buffer, "*%c%d - %d%%, t: %ds, dry: %d, wet: %d",
          (char)(in + OFFSET_ASCII_L),
          hs.minSoil,
          hs.maxSoil,
          hs.timeOnPomp,
          hs.dry,
          hs.wet);

  Serial.println(buffer);
}