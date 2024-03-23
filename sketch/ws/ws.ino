#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define VERSION "Version: 0.0.2"

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DEBUG
#define LCD

#define DRY 460
#define WET 190

#define SPEED_SERIAL 9600
#define OFFSET_ASCII 65

#define SIZE 4                // string for "Bluetooth Electronics"
#define STAR '*'              // const for "Bluetooth Electronics"
#define PERIOD_ON_POMPA 5000  // to config (time on pompa)

#define SIZE_AIO 4                 // size AIO Uno
#define OFFSET_FOR_SOLENOID_PIN 2  // skip RX, TX for bluetooth
char size_average = -1;            // to config (count measurements)

int counter_measuring = 0;  // depends COUNT_AVERAGE
int averages[SIZE_AIO];
char array[SIZE];  // for convert to "Bluetooth Electronics"

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

void init_eeprom(){
  CommonSettings cs;
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

  CommonSettings cs;
  EEPROM.get(0, cs);
  Serial.print("counter : ");
  Serial.println(cs.counter_of_size);
  
  Serial.print("[0]: ");
  Serial.println(cs.settings[0].enable);

  size_average = cs.counter_of_size;

  Serial.print("Size averages: ");
  Serial.println((int)size_average);

  pinMode(toSolenoidPin(A), OUTPUT);
  pinMode(toSolenoidPin(B), OUTPUT);
  pinMode(toSolenoidPin(C), OUTPUT);
  pinMode(toSolenoidPin(D), OUTPUT);

  digitalWrite(toSolenoidPin(A), HIGH);
  digitalWrite(toSolenoidPin(B), HIGH);
  digitalWrite(toSolenoidPin(C), HIGH);
  digitalWrite(toSolenoidPin(D), HIGH);

  resetAvg();  // inline

#ifdef DEBUG
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Watering system");
  lcd.setCursor(0, 1);
  lcd.print(VERSION);
#endif
  Serial.begin(SPEED_SERIAL);
}

void loop() {

  averages[A] += map(analogRead(A0), DRY, WET, 0, 100);
  averages[B] += map(analogRead(A1), DRY, WET, 0, 100);
  averages[C] += map(analogRead(A2), DRY, WET, 0, 100);
  averages[D] += map(analogRead(A3), DRY, WET, 0, 100);

  counter_measuring++;

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

  if (size_average == counter_measuring) {
    const int avg_0 = averages[A] / size_average;
    const int avg_1 = averages[B] / size_average;
    const int avg_2 = averages[C] / size_average;
    const int avg_3 = averages[D] / size_average;

    print(0, 1, avg_0, A);   // TODO improve
    print(4, 1, avg_1, B);   //
    print(8, 1, avg_2, C);   //
    print(12, 1, avg_3, D);  //
    Serial.println(convert(avg_0, A));
    Serial.println(convert(avg_1, B));
    Serial.println(convert(avg_2, C));
    Serial.println(convert(avg_3, D));

    check(avg_0, toSolenoidPin(A));  // for  testing using now only A and D
    //check(avg_1, toSolenoidPin(B));
    //check(avg_2, toSolenoidPin(C));
    check(avg_3, toSolenoidPin(D));

    resetAvg();
    counter_measuring = 0;
    delay(1000);
  }
}

inline void resetAvg() {
  memset(averages, 0, sizeof(int) * SIZE_AIO);
}

inline int toSolenoidPin(IN val) {
  return val + OFFSET_FOR_SOLENOID_PIN;
}

void print(const int lcdL, const int lcdC, const int val, const IN pin) {
  char buff[5];
  buff[0] = convertToChar(pin);
  setConstrain(&val);
  char length = strlen(itoa(val, buff + 1, 10)) + 1;
  memset(buff + length, 0x20, 5 - length);
  lcd.setCursor(lcdL, lcdC);
  lcd.print(buff);
}

inline void setConstrain(int *pval) {
  if (*pval < 0)
    *pval = 0;
  if (*pval > 99)
    *pval = 99;
}

inline char convertToChar(IN pin) {
  return pin + OFFSET_ASCII;
}

char *convert(int val, char pin) {
  memset(array, 0, SIZE);
  array[0] = STAR;
  array[1] = convertToChar(pin);
  const int length = strlen(itoa(val, array + 2, 10));
  array[length + 2] = STAR;
  return array;
}

void check(int val, int out) {
  if (val < 40) {
    digitalWrite(out, LOW);
    delay(PERIOD_ON_POMPA);
    digitalWrite(out, HIGH);
  }
}