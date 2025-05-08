#include <LiquidCrystal.h>
#include <Stepper.h>
#include <RTClib.h>
#include <dht.h>

#define RDA 0x80
#define TBE 0x20
#define DHT11_PIN 34
#define FAN_MOTOR_ENABLE 9

volatile unsigned char *portA = (unsigned char *) 0x22;
volatile unsigned char *ddrA  = (unsigned char *) 0x21;
volatile unsigned char *pinA  = (unsigned char *) 0x20;

volatile unsigned char *portC = (unsigned char *) 0x28;
volatile unsigned char *ddrC  = (unsigned char *) 0x27;
volatile unsigned char *pinC  = (unsigned char *) 0x26;

volatile unsigned char *ddrD = (unsigned char *) 0x2A;

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

// Start/Stop Button
volatile unsigned long lastInterruptTime = 0; // Last time the start/stop button was pressed.
const unsigned long debounceDelay = 100; // Debounce delay for the start/stop button.

// LCD Screen
const int RS = 11, EN = 12, D4 = 2, D5 = 3, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);
const unsigned long LCD_UPDATE_INTERVAL = 10000; // 1 minute interval for updating the LCD.
unsigned long lastLCDUpdate = 0; // Last time the LCD was updated using millis().

// RTC (Real Time Clock)
char dayNames[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
RTC_DS3231 rtc;
bool logStateTrigger = false; // Used to trigger a state log to the serial monitor when the start/stop button is pressed.

// DHT11 (Temperature and Humidity Sensor)
dht DHT;

// Stepper Motor
const char stepperStateNames[4][7] = {"MIDDLE", "UP", "MIDDLE", "DOWN"};
const int stepperStates[] = {0, 300, 0, -300}; // Stepper motor position associated with each stepper state.
Stepper myStepper = Stepper(2038, 28, 30, 29, 31);
int stepperState = 0; // Keeps track of the current stepper state.

// Stepper Button
bool previousStepperButtonState = false; // Previous state of the stepper button for rising edge detection.

// States
const char stateNames[4][9] = {"DISABLED", "IDLE", "ERROR", "RUNNING"};
enum State {
  DISABLED = 0,
  IDLE = 1,
  ERROR = 2,
  RUNNING = 3
};
State state;

// Other Variables
const unsigned int WATER_THRESHOLD = 10; // Water level threshold requirement for the system to run.
const unsigned int TEMP_THRESHOLD = 80; // Temperature threshold in Fahrenheit to begin cooling.

void setup() {
  // Start/Stop Button
  *ddrD &= ~(1 << 3); // Set pin 18 as input.
  attachInterrupt(digitalPinToInterrupt(18), startButtonISR, FALLING); // Clicking the start/stop button will trigger an interrupt by calling startButtonISR().

  // Reset Button
  *ddrA &= ~(1 << 5); // Set pin 27 as input.

  // LCD Screen
  *ddrA |= (1 << 0);
  *ddrA |= (1 << 1);
  *ddrA |= (1 << 2);
  *ddrA |= (1 << 3);
  lcd.begin(16, 2);

  // RTC
  rtc.begin();
  
  // Water Sensor
  adcInit();

  // Fan Motor
  *ddrC |= (1 << 5); // DIR2: Pin 32
  *ddrC |= (1 << 4); // DIR1: Pin 33
  *ddrD |= (1 << 7); // FAN_MOTOR_ENABLE: Pin 9

  // Stepper Button
  *ddrC &= ~(1 << 1); // Set pin 36 as input.

  // UART Serial Communication
  U0init(9600); // Initialize UART with 9600 baud rate.

  // General Setup
  changeState(DISABLED);
  logState();
} 

void loop() {
  checkLogStateTrigger();

  if (state == DISABLED) return;

  // Change the stepper motor position when the stepper button is pressed.
  bool stepperButtonState = (*pinC & (1 << 1)) != 0;
  if (stepperButtonState && !previousStepperButtonState) {
    changeStepper();
  }
  previousStepperButtonState = stepperButtonState;

  // Read temperature and humidity.
  DHT.read11(DHT11_PIN);
  float temp = DHT.temperature * 9.0 / 5.0 + 32;
  float humidity = DHT.humidity;

  // Read water level.
  unsigned int waterLevel = adcRead(0);

  if (state == IDLE) {
    checkLogStateTrigger();
    updateLCD(temp, humidity);

    // Change to the ERROR state if the water level is below the threshold.
    if (waterLevel < WATER_THRESHOLD) {
      changeState(ERROR);
      logState();
    }

    // Change to the RUNNING state if the temperature is above the threshold.
    if (temp > TEMP_THRESHOLD) {
      changeState(RUNNING);
      logState();
    }
  } else if (state == ERROR) {
    checkLogStateTrigger();

    // Change to the IDLE state if the reset button is pressed and the water level is above the threshold.
    int resetButtonState = *pinA & (1 << 5);
    if (resetButtonState != 0 && waterLevel >= WATER_THRESHOLD) {
      changeState(IDLE);
      logState();
    }
  } else if (state == RUNNING) {
    checkLogStateTrigger();
    updateLCD(temp, humidity);

    // Change to the ERROR state if the water level is below the threshold.
    if (waterLevel < WATER_THRESHOLD) {
      changeState(ERROR);
      logState();
    }

    // Change to the IDLE state if the temperature is below the threshold.
    if (temp <= TEMP_THRESHOLD) {
      changeState(IDLE);
      logState();
    }
  }
}

// Perform some state initialization steps when changing states.
void changeState(State change_state) {
  state = change_state;
  activateLED();
  
  if (state == DISABLED) {
    clearLCD();
    stopMotor();
  } else if (state == IDLE) {
    resetLCDTimer();
    stopMotor();
  } else if (state == ERROR) {
    errorLCD();
    stopMotor();
  } else if (state == RUNNING) {
    resetLCDTimer();
    startMotor();
  }
}

// Start/Stop button interrupt.
void startButtonISR() {
  // Uses button debounce to avoid accidental button spam.
  if (millis() - lastInterruptTime > debounceDelay) {
    // Change to the IDLE state if the system is disabled.
    if (state == DISABLED) {
      changeState(IDLE);
      logStateTrigger = true;
    // Change to the DISABLED state otherwise.
    } else {
      changeState(DISABLED);
      logStateTrigger = true;
    }

    lastInterruptTime = millis();
  }
}

// Turn on the LED corresponding to the current state.
void activateLED() {
  *portA &= ~(1 << DISABLED);
  *portA &= ~(1 << IDLE);
  *portA &= ~(1 << ERROR);
  *portA &= ~(1 << RUNNING);
  *portA |= (1 << state);
}

// Shows the current temperature and humidity on the LCD screen.
void updateLCD(float temp, float humidity) {
  // Only update if the timer is over or if the timer was recently reset.
  if (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    lastLCDUpdate = millis();
    clearLCD();

    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print("F");

    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.print("%");
  }
}

// Shows the error message on the LCD screen.
void errorLCD() {
  lcd.setCursor(0, 0);
  lcd.write("ERROR: Water Low");
  lcd.setCursor(0, 1);
  lcd.write("                ");
}

// Clears the LCD screen.
void clearLCD() {
  lcd.setCursor(0, 0);
  lcd.write("                ");
  lcd.setCursor(0, 1);
  lcd.write("                ");
}

// Resets the LCD update timer so that it will update immediately.
void resetLCDTimer() {
  lastLCDUpdate = millis() + LCD_UPDATE_INTERVAL;
}

// Prints the current state to the serial monitor.
void logState() {
  logTime();
  U0putString(" -> State: ");
  U0putString(stateNames[state]);
  U0putchar('\n');
}

// Prints the current stepper state to the serial monitor.
void logStepper() {
  logTime();
  U0putString(" -> Stepper State: ");
  U0putString(stepperStateNames[stepperState]);
  U0putchar('\n');
}

// Prints the current time to the serial monitor.
void logTime() {
  DateTime now = rtc.now();

  U0putString(toString(now.year()));
  U0putchar('/');
  U0putString(toString(now.month()));
  U0putchar('/');
  U0putString(toString(now.day()));
  U0putString(" (");
  U0putString(dayNames[now.dayOfTheWeek()]);
  U0putString(") ");
  U0putString(toString(now.hour()));
  U0putchar(':');
  U0putString(toString(now.minute()));
  U0putchar(':');
  U0putString(toString(now.second()));
}

// Logs the current state from the loop() function as a result of an ISR state change.
void checkLogStateTrigger() {
  if (logStateTrigger) {
    logState();
    logStateTrigger = false;
  }
}

// Initializes the ADC.
void adcInit() {
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

// Reads from the ADC.
unsigned int adcRead(unsigned char adc_channel_num) {
  *my_ADMUX &= ~0x1F;
  *my_ADCSRB &= ~0x08;
  *my_ADMUX |= adc_channel_num & 0x07;
  *my_ADCSRA |= 0x40;
  while((*my_ADCSRA & 0x40) != 0);
  
  unsigned int val = *my_ADC_DATA & 0x3FF;
  return val;
}

// Starts the fan motor.
void startMotor() {
  *portC |= (1 << 4);
  *portC &= ~(1 << 5);
  analogWrite(FAN_MOTOR_ENABLE, 250);
}

// Stops the fan motor.
void stopMotor() {
  analogWrite(FAN_MOTOR_ENABLE, 0);
}

// Changes the stepper state resulting in a change of position.
void changeStepper() {
  int currentStepperPosition = stepperStates[stepperState];
  stepperState = (stepperState + 1) % 4;
  logStepper();

  myStepper.setSpeed(10);
  myStepper.step(stepperStates[stepperState] - currentStepperPosition);
}

// Initializes the UART serial communication.
void U0init(int U0baud) {
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);

  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0  = tbaud;

  U0putchar('\n');
}

// Prints a character to the serial monitor.
void U0putchar(unsigned char U0pdata) {
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

// Prints a string to the serial monitor.
void U0putString(char* U0pstring) {
  while (*U0pstring != '\0') {
    U0putchar(*U0pstring);
    U0pstring += 1;
  }
}

// Converts an integer to a string.
char* toString(int num) {
  static char string[100];
  int i = 0;

  // Place integer digits into the string as characters in reverse order.
  if (num == 0) {
    string[i] = '0';
    i += 1;
  } else {
    while (num > 0) {
      string[i] = (num % 10) + '0';
      num /= 10;
      i += 1;
    }
  }

  // Reverse the string to get the correct order.
  int left = 0;
  int right = i - 1;
  while (left < right) {
    char temp = string[left];
    string[left] = string[right];
    string[right] = temp;

    left += 1;
    right -= 1;
  }

  string[i] = '\0';
  return string;
}