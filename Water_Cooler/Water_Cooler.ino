/*
Authors: Aidan Kirk, Gisselle Cruz-Robinson, John Michael-Libed
*/
#include <LiquidCrystal.h>
#include <Stepper.h>
#include <dht.h>

#define RDA 0x80
#define TBE 0x20  
#define DHT11_PIN 34

// Start/Stop Button
volatile unsigned char *portD  = (unsigned char *) 0x2B;
volatile unsigned char *ddrD   = (unsigned char *) 0x2A;
volatile unsigned char *pinD   = (unsigned char *) 0x29;

// LEDs
volatile unsigned char *portA  = (unsigned char *) 0x22;
volatile unsigned char *ddrA   = (unsigned char *) 0x21;
volatile unsigned char *pinA   = (unsigned char *) 0x20;

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

// States
enum State {
  DISABLED = 0,
  IDLE = 1,
  ERROR = 2,
  RUNNING = 3
};
State state = DISABLED;

//Stepper Motor Revolutions + pin mapping
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 28, 29, 30, 31);

//DHT11
dht DHT;

//LCD pin mapping
const int RS = 11, EN = 12, D4 = 2, D5 = 3, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

const unsigned int WATER_LOWER_THRESHOLD = 10;

void setup() {
  Serial.begin(9600);

  // Configure start/stop button and interrupt state.
  *ddrD &= ~(1 << 3); // Set pin 18 as input
  attachInterrupt(digitalPinToInterrupt(18), startButtonISR, FALLING);

  // Configure LEDs.
  *ddrA |= (1 << 0);
  *ddrA |= (1 << 1);
  *ddrA |= (1 << 2);
  *ddrA |= (1 << 3);

  // LCD displaying current air temperature and humidity
  lcd.begin(16, 2);
  
  // Configure water sensor.
  U0init(9600);
  adc_init();
}

void loop() {
  if (state == DISABLED) {
    activateLED(DISABLED);

    lcd.setCursor(0, 0);
    lcd.write("                ");
    lcd.setCursor(0, 1);
    lcd.write("                ");
  } else {
    if (state == IDLE) {
      activateLED(IDLE);
      unsigned int waterLevel = adc_read(0);

      if (waterLevel < WATER_LOWER_THRESHOLD) {
        state = ERROR;

        lcd.setCursor(0, 0);
        lcd.write("ERROR: Water Low");
        lcd.setCursor(0, 1);
        lcd.write("                ");
      } else {
        lcd.setCursor(0, 0);
        lcd.write("Temp: ");
        lcd.setCursor(0, 1);
        lcd.write("Humidity: ");
      }
    } else if (state == ERROR) {
      activateLED(ERROR);
    } else if (state == RUNNING) {
      activateLED(RUNNING);
    }
  }
}

// Start/stop button triggers an interrupt.
// If the current state is DISABLED, we switch to the IDLE state.
// Otherwise, we switch to the DISABLED state.
void startButtonISR() {
  if (state == DISABLED) {
    state = IDLE;
  } else {
    state = DISABLED;
  }
}

void activateLED(State led_state) {
  *portA &= ~(1 << DISABLED);
  *portA &= ~(1 << IDLE);
  *portA &= ~(1 << ERROR);
  *portA &= ~(1 << RUNNING);
  *portA |= (1 << led_state);
}

void adc_init() {
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

unsigned int adc_read(unsigned char adc_channel_num) {
  *my_ADMUX &= ~0x1F;
  *my_ADCSRB &= ~0x08;
  *my_ADMUX |= adc_channel_num & 0x07;
  *my_ADCSRA |= 0x40;
  while((*my_ADCSRA & 0x40) != 0);
  
  unsigned int val = *my_ADC_DATA & 0x3FF;
  return val;
}

void U0init(int U0baud) {
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);

  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0  = tbaud;
}

unsigned char U0kbhit() {
  return *myUCSR0A & RDA;
}

unsigned char U0getchar() {
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata) {
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}