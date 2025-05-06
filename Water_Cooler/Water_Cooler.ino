/*
Authors: Aidan Kirk, Gisselle Cruz-Robinson, John Michael-Libed
*/
#include <LiquidCrystal.h>
#include <Stepper.h>
#include <dht.h>
#include <RTClib.h>

#define RDA 0x80
#define TBE 0x20 
#define DHT11_PIN 34
#define ENABLE 38

// Start/Stop Button
volatile unsigned char *ddrD = (unsigned char *) 0x2A;

// LEDs and Reset Button
volatile unsigned char *portA = (unsigned char *) 0x22;
volatile unsigned char *ddrA  = (unsigned char *) 0x21;
volatile unsigned char *pinA  = (unsigned char *) 0x20;

// ADC Pointers
volatile unsigned char *my_ADMUX    = (unsigned char *) 0x7C;
volatile unsigned char *my_ADCSRB   = (unsigned char *) 0x7B;
volatile unsigned char *my_ADCSRA   = (unsigned char *) 0x7A;
volatile unsigned int  *my_ADC_DATA = (unsigned int  *) 0x78;

// UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *) 0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *) 0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *) 0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int  *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *) 0x00C6;

// Fan Motor
volatile unsigned char *portC = (unsigned char *) 0x28;
volatile unsigned char *ddrC = (unsigned char *) 0x27;
volatile unsigned char *pinC = (unsigned char *) 0x26;

// States
enum State {
  DISABLED = 0,
  IDLE = 1,
  ERROR = 2,
  RUNNING = 3
};
State state = DISABLED;

// Stepper Motor Revolutions + pin mapping
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 28, 29, 30, 31);

//Stepper Motor Control
int currentStepperPosition = 0;

// DHT11
dht DHT;

// LCD pin mapping
const int RS = 11, EN = 12, D4 = 2, D5 = 3, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

const unsigned int WATER_THRESHOLD = 10;
const unsigned int TEMP_THRESHOLD = 80;

// Timer used for LCD updates.
const unsigned long LCD_UPDATE_INTERVAL = 60000; // 1 minute
unsigned long lastLCDUpdate = 0;

void setup() {
  Serial.begin(9600);

  // Configure start/stop button and interrupt state.
  *ddrD &= ~(1 << 3); // Set pin 18 as input
  attachInterrupt(digitalPinToInterrupt(18), startButtonISR, FALLING);

  // Configure reset button.
  *ddrA &= ~(1 << 5); // Set pin 27 as input

  // Configure LEDs.
  *ddrA |= (1 << 0);
  *ddrA |= (1 << 1);
  *ddrA |= (1 << 2);
  *ddrA |= (1 << 3);

  // Configure Fan Motor Pins

  *ddrC &= ~(1 << 1); // PIN 36
  *ddrC &= ~(1 << 0); // PIN 37

  //0010 0000
  *ddrC |= (1 << 5); //DIR2: Pin 32
  *ddrC |= (1 << 4); //DIR1: Pin 33
  *ddrD |= (1 << 7); //ENABLE: Pin 38

  // LCD displaying current air temperature and humidity
  lcd.begin(16, 2);
  
  // Configure water sensor.
  U0init(9600);
  adc_init();
}

void loop() {
  DHT.read11(DHT11_PIN);
  float temp = DHT.temperature;
  float humidity = DHT.humidity;

  unsigned int waterLevel = adc_read(0);
  int resetButtonState = *pinA & (1 << 5);

  if (state == DISABLED) {
    activateLED(DISABLED);
    clearLCD();
  } else if (state == IDLE) {
    activateLED(IDLE);
    updateLCD(temp, humidity);

    if (waterLevel < WATER_THRESHOLD) {
      state = ERROR;
    }

    if (temp > TEMP_THRESHOLD) {
      state = RUNNING;
      lastLCDUpdate = 0;
    }
  } else if (state == ERROR) {
    activateLED(ERROR);
    errorLCD();

    if (resetButtonState == 0 && waterLevel >= WATER_THRESHOLD) {
      state = IDLE;
      lastLCDUpdate = 0;
    }
  } else if (state == RUNNING) {
    activateLED(RUNNING);
    updateLCD(temp, humidity);

    if (waterLevel < WATER_THRESHOLD) {
      state = ERROR;
    }

    if (temp <= TEMP_THRESHOLD) {
      state = IDLE;
      lastLCDUpdate = 0;
    }
  }
}

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

void clearLCD() {
  lcd.setCursor(0, 0);
  lcd.write("                ");
  lcd.setCursor(0, 1);
  lcd.write("                ");
}w

void updateLCD(float temp, float humidity) {
  if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    lastLCDUpdate = millis();
    clearLCD();

    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.print("%");
  }
}

void errorLCD() {
  lcd.setCursor(0, 0);
  lcd.write("ERROR: Water Low");
  lcd.setCursor(0, 1);
  lcd.write("                ");
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

//Stepper Control
void controlStepper() {
  int controlValue = directionControl();
        
  if (controlValue != 0 && state != DISABLED) {
    int stepsToMove = controlValue * 10;
    myStepper.step(stepsToMove);
    currentStepperPosition += controlValue * 5;
  }
}

int directionControl() {
    int pin36 = (*pinC & (1 << 1)) ? 1 : 0; // Read pin 36 (PC1)
    int pin37 = (*pinC & (1 << 0)) ? 1 : 0; // Read pin 37 (PC0)
    
    if (pin36 && !pin37) {
        return 1; // Clockwise
    } else if (!pin36 && pin37) {
        return -1; // Counterclockwise
    } else {
        return 0;
    }
}

//Fan motor functions
void startMotor(){
  *portC |= 0x20; //Set DIR1 high
  *portC &= 0x10; //Set DIR2 low
  analogWrite(ENABLE, 250);
}

void stopMotor(){
  analogWrite(ENABLE, 0);
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