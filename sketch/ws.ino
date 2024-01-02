/*

*/
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

#define DEBUG

#define DRY 800
#define WET 401

#define SPEED_SERIAL 9600

void setup() {
  lcd.init();
  lcd.backlight();
  
#ifdef DEBUG
  Serial.begin(SPEED_SERIAL);
#endif
}

void loop() {
  print(0, 0, analogRead(A0));
  print(0, 4, analogRead(A1));
  print(0, 8, analogRead(A2));
  print(0, 12, analogRead(A3));

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
  delay(1000);
}

void print(int lcdL, int lcdC, int val){
  char buff[4];
  memset(buff, 32, 4);
  lcd.setCursor(lcdC,lcdL);

  if(WET <= val){
    int len = strlen(itoa(map(val, WET, DRY, 100, 0), buff, 10));
    buff[len] = '%';
    lcd.print(buff);
  }
  else{
    buff[1] = '?';
    buff[2] = '?';
    lcd.print(buff);
  }
}
