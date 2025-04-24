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
  
  //WATER SENSOR STUFF
  U0init(9600);
  adc_init();
}

void loop() {
  
  unsigned int waterLevel = adc_read(0);
  const unsigned int THRESHOLD = 200;
  const unsigned int LOWER_THRESHOLD = 10;

  //ADD A CHECK FOR LED STATES ONLY SHOULD BE RUNNING IF NOT DISABLED
  //DONT RUN IF YELLOW LED IS ENABLED
  if (waterLevel < THRESHOLD) {
    if (waterLevel >= 1000) {
      U0putchar('0' + (waterLevel / 1000));
      U0putchar('0' + ((waterLevel / 100) % 10));
      U0putchar('0' + ((waterLevel / 10) % 10));
      U0putchar('0' + (waterLevel % 10));
    }
    else if (waterLevel >= 100) {
      U0putchar('0' + (waterLevel / 100));
      U0putchar('0' + ((waterLevel / 10) % 10));
      U0putchar('0' + (waterLevel % 10));
    }
    else if (waterLevel >= 10) {
      U0putchar('0' + (waterLevel / 10));
      U0putchar('0' + (waterLevel % 10));
    }
    else {
      U0putchar('0' + waterLevel);
    }
    
    U0putchar('\r');
    U0putchar('\n');
  } 
  else if (waterLevel < LOWER_THRESHOLD) {
    //ERROR STATE WHEN WATER BECOMES LOW
    //CHANGE STATE AND PRINT OUT ERROR
  }
  else {
    //CHANGE OUT TO LCD 

    const char* message = "Water Level: High";
    for (int i = 0; message[i] != '\0'; i++) {
      U0putchar(message[i]);
    }
    U0putchar('\r');
    U0putchar('\n');
  }
}

//RETRIVE LED PIN STATES
//BLUE = RUNNING
//RED = ERROR
//GREEN = IDLE
//YELLOW = DISABLED
void setCoolerState();
//string getCoolerState();
//ADD ENUM STATES FOR COOLER STATES

void adc_init() //write your code after each commented line and follow the instruction 
{
 *my_ADCSRA |= 0x80;
 *my_ADCSRA &= ~0x40;
 *my_ADCSRA &= ~0x20;
 *my_ADCSRA &= ~0x07;
 *my_ADCSRB &= ~0x08;
 *my_ADCSRB &= ~0x07;
 *my_ADMUX &= ~0x80;
 *my_ADMUX |= 0x40;
 *my_ADMUX &= ~0x20;
 *my_ADMUX &= ~0x1F;
}

unsigned int adc_read(unsigned char adc_channel_num)
{
  *my_ADMUX &= ~0x1F;
  *my_ADCSRB &= ~0x08;
  *my_ADMUX |= adc_channel_num & 0x07;
  *my_ADCSRA |= 0x40;
  while((*my_ADCSRA & 0x40) != 0);
  
  unsigned int val = *my_ADC_DATA & 0x3FF;
  return val;
}

void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}



