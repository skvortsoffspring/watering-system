#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define VERSION "Version: 0.0.2"

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DEBUG
#define LCD

#define DRY 460
#define WET 190

#define SPEED_SERIAL 9600
#define OFFSET_ASCII 65


#define SIZE 6                      // string for "Bluetooth Electronics"
#define STAR '*'                    // const for "Bluetooth Electronics"
#define PERIOD_ON_POMPA 5000        // to config (time on pompa)

#define SIZE_AIO 6                  // size AIO Uno
#define OFFSET_FOR_SOLENOID_PIN 2   // skip RX, TX for bluetooth
#define COUNT_AVERAGE 10             // to config (count measurements)

int counter_measuring = 0;             // depends COUNT_AVERAGE
int averages[SIZE_AIO];
char array[SIZE];                    // for convert to "Bluetooth Electronics"

enum IN { A, B, C, D };

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);

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

  if (COUNT_AVERAGE == counter_measuring) {
    const int avg_0 = averages[A] / COUNT_AVERAGE;
    const int avg_1 = averages[B] / COUNT_AVERAGE;
    const int avg_2 = averages[C] / COUNT_AVERAGE;
    const int avg_3 = averages[D] / COUNT_AVERAGE;

    print(0, 1, avg_0, A);    // TODO improve
    print(4, 1, avg_1, B);    //
    print(8, 1, avg_2, C);    //
    print(12, 1, avg_3, D);   //
 
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
  }
  delay(1000);
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
  memset(buff + length , 0x20, 5 - length);
  lcd.setCursor(lcdL, lcdC);
  lcd.print(buff);
}

inline void setConstrain(int * pval){
  if(*pval < 0) *pval = 0;
  if(*pval > 100) *pval = 100;
}

inline char convertToChar(IN pin) {
  return pin + OFFSET_ASCII;
}

char* convert(int val, char pin) {
  memset(array, 0, SIZE);
  array[0] = STAR;
  array[1] = convertToChar(pin);
  const int length  = strlen(itoa(val, array + 2, 10));
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