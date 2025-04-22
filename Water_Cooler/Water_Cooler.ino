/*
Authors: Aiden Kirk, Gisselle Cruz-Robinson, John Michael-Libed
*/
#include <LiquidCrystal.h>
#include <Stepper.h>
#include <dht.h>

#define RDA 0x80
#define TBE 0x20  
#define DHT11_PIN 34

//ADC Pointers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

//Stepper Motor Revolutions + pin mapping
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 28, 29, 30, 31);

//DHT11
dht DHT;

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
