/*
Authors: Aiden Kirk, Gisselle Cruz-Robinson, John Michael-Libed
*/
#include <LiquidCrystal.h>

//ADC Pointers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//LCD pin mapping
const int RS = 11, EN = 12, D4 = 2, D5 = 3, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

void setup() {
  // LCD displaying current air temperature and humidity
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.write("Temp: ");
  lcd.setCursor(0, 1);
  lcd.write("Humidity: ");
}

void loop() {
  
}
