#include <EEPROM.h>
#include <LiquidCrystal.h>
#include "ClickEncoder.h"

//#define __DEBUG
#ifdef __DEBUG
#  define __SERIAL(msg)     {Serial.print(msg);}
#  define __SERIAL_LN(msg)   {Serial.println(msg);}
#else
#  define __SERIAL(msg)
#  define __SERIAL_LN(msg)
#endif

// Menu setup
#define MENU_ITEMS      3
#define MENU_PNUM       0
#define MENU_PSHIFT     1
#define MENU_PDURATION  2

int16_t menuData[MENU_ITEMS] = {1, 50, 1000};
const int16_t menuDataMax[MENU_ITEMS] = {9, 90, 30000};
const int8_t menuDataMin[MENU_ITEMS] = {1, 10, 0};
const int8_t menuIncrement[MENU_ITEMS] = {1, 1, 10};

int8_t menuItem = 0;  // Currently selected item
const uint16_t menuTimeout = 5000; // ms

// UI setup
#define MODE_READY  0
#define MODE_MENU   1

#define __MODE_READY (modeUI == MODE_READY)
#define __MODE_MENU  (modeUI == MODE_MENU)

uint8_t modeUI = MODE_READY;

#define TIMER_NUM 5
#define __TIMEOUT(n,a) {timer[(n)] = millis(); timerDelay[(n)] = (a);}
#define __TCHECK(n) ((millis()-timer[(n)])>timerDelay[(n)])
uint32_t timer[TIMER_NUM];           // timeout start point
uint16_t timerDelay[TIMER_NUM];      // UI timout in ms

#define BLINK_ON_TIME   700   // ms
#define BLINK_OFF_TIME  300   // ms

#define __BLINK_START   { bBlink = true; __TIMEOUT(1, BLINK_ON_TIME); }
#define __BLINK_CHANGE  { bBlink ^= 1; __TIMEOUT(1, (bBlink) ? BLINK_ON_TIME : BLINK_OFF_TIME); }
#define __BLINK          __TCHECK(1)
bool bBlink;

#define __HOLD    {bHold  = true;}
#define __UNHOLD  {bHold  = false;}
bool bHold;

// UI update flag
#define UPDATE_NONE  0
#define UPDATE_1ST   1
#define UPDATE_2ND   2
#define UPDATE_ALL   3

#define __UPDATE_ALL {needUpdateUI = UPDATE_ALL; }
#define __UPDATE(mi) {needUpdateUI = (mi<MENU_PDURATION)?UPDATE_1ST:UPDATE_2ND;}
uint8_t needUpdateUI = UPDATE_ALL;

#define __ACTIVE  { __TIMEOUT(0, menuTimeout); __SERIAL("Set active state. Timeout is ");  __SERIAL_LN(menuTimeout); }
#define __IDLE    __TCHECK(0)

// Setup encoder
const uint8_t stepsPerNotch = 2;
const uint8_t pinA   = 9;
const uint8_t pinB   = A4;
const uint8_t pinCLK = A5;
int16_t encoderIncrement;

ClickEncoder *encoder;
ClickEncoder::Button encoderButton;


// Pins definition
const uint8_t pinWeld = 4;
const uint8_t pinFan  = 3;
const uint8_t pinFire = 8;
const uint8_t pinZero = 2;

volatile bool flagFire = false;
volatile bool flagZero = false;
volatile uint32_t zeroCrossTime = 1000000000L;


// Use timeout before turning fan off after welding
#define __FAN_START      { flagFan=true;  digitalWrite(pinFan, HIGH); }
#define __FAN_STOP       { flagFan=false; digitalWrite(pinFan, LOW);  }
#define __FAN_DELAY      __TIMEOUT(3, 10000)
#define __FAN_TIMEOUT    __TCHECK(3)
bool flagFan = false;

// Setup LCD display
LiquidCrystal lcd(A2, A1, A0, 13, 12, 11);

void setup()
{
  // Start LCD
  lcd.begin(16, 2);
  lcd.clear();

#ifdef __DEBUG
  Serial.begin(9600);
#endif

  // Setup pins
  pinMode(pinFire, INPUT_PULLUP);

  pinMode(pinFan, OUTPUT);
  digitalWrite(pinFan, LOW);

  pinMode(pinWeld, OUTPUT);
  digitalWrite(pinWeld, LOW);

  pinMode(pinZero, INPUT);
  attachInterrupt(digitalPinToInterrupt(pinZero), zeroCrossInterrupt, RISING);

  // Start encoder
  encoder = new ClickEncoder(pinA, pinB, pinCLK, stepsPerNotch);

  // Timer0 is already used for millis()
  // We'll use that for encoder->service() routine and button tracking
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);

  // Load setings from EEPROM
  eepromLoad();

  // Show splash screen
  lcd.setCursor(0, 0);   lcd.print("  Spot Welding");
  lcd.setCursor(0, 1);   lcd.print("  2016 (C) LOM");
  delay(1000);

  // Setup flags & modes
  modeUI = MODE_READY;
  flagFire = false;
  flagZero = false;
} // setup()

void loop()
{
  // Check if fan is turned on and switch it off by timout
  if ( flagFan && __FAN_TIMEOUT ) __FAN_STOP;

  // Check if weld button is pressed & do the things right
  if (flagFire) {
    const int ms = menuData[MENU_PDURATION];
    const int us = 100 * menuData[MENU_PSHIFT] + 300;
    // Pause between pulses will be equal to pulse time but not less than 100ms
    const int pause = max(ms, 100);  

    __FAN_START;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ignite in 3 sec ");
    delay(1000);
    lcd.setCursor(0, 0);
    lcd.print("Ignite in 2 sec ");
    delay(1000);
    lcd.setCursor(0, 0);
    lcd.print("Ignite in 1 sec ");
    delay(1000);

    for (int i = 0; i < menuData[MENU_PNUM]; i++, delay(pause)) {
      lcd.setCursor(0, 0);
      lcd.print("Ignition #      ");
      lcd.setCursor(10, 0);
      lcd.print(i + 1);

      // Wait for zero cross
      flagZero = false;
      while (!flagZero)
        ; // empty cycle

      // Weld it nice & easy! :)
      delayMicroseconds(us);
      digitalWrite(pinWeld, HIGH);
      delay(ms);
      digitalWrite(pinWeld, LOW);
    }

    lcd.setCursor(0, 0);
    lcd.print("Done!           ");
    delay(1000);

    __UPDATE_ALL;
    flagFire = false;
    __FAN_DELAY;
  }


  // Get encoder shaft state
  encoderIncrement = -encoder->getValue();
  if (encoderIncrement && __MODE_MENU) {
    __SERIAL("Encoder increment: ");
    __SERIAL_LN(encoderIncrement);

    menuData[menuItem] += encoderIncrement * menuIncrement[menuItem];

    __SERIAL("New parameter value: ");
    __SERIAL_LN(menuData[menuItem]);

    if ( menuData[menuItem] > menuDataMax[menuItem] )
      menuData[menuItem] = menuDataMax[menuItem];

    if ( menuData[menuItem] < menuDataMin[menuItem] )
      menuData[menuItem] = menuDataMin[menuItem];

    __UPDATE(menuItem);
    __ACTIVE;
  } // if

  // Get encoder shaft state
  encoderButton = encoder->getButton();
  if (encoderButton != ClickEncoder::Open) {
    __SERIAL_LN("Button event happened");
    __ACTIVE;

    switch (encoderButton) {
      case ClickEncoder::Pressed:
        break;
      case ClickEncoder::Held:
        if ( __MODE_READY && !bHold) {
          __SERIAL_LN("Mode changed to MENU by User");

          modeUI = MODE_MENU;
          menuItem = 0;
          __UPDATE_ALL;
          __BLINK_START;
          __HOLD;
        }
        if ( __MODE_MENU && !bHold) {
          __SERIAL_LN("Mode changed to READY by User");

          modeUI = MODE_READY;
          __UPDATE_ALL;
          eepromSave();
          __HOLD;
        }
        break;
      case ClickEncoder::Released:
        __UNHOLD;
        break;
      case ClickEncoder::Clicked:
        if ( __MODE_MENU ) {
          menuItem = ++menuItem % MENU_ITEMS;
          __UPDATE_ALL;
          __BLINK_START;
        }
        break;
      case ClickEncoder::DoubleClicked:
        if ( __MODE_MENU ) {
          menuItem = ++(++menuItem) % MENU_ITEMS;
          __UPDATE_ALL;
          __BLINK_START;
        }
        break;
    }
  } // if

  // Exit menu mode by timout
  if ( __MODE_MENU && __IDLE) {
    __SERIAL_LN("Mode changed to READY by timeout");
    modeUI = MODE_READY;
    eepromSave();
    __UPDATE_ALL;
  }

  // Switch blink state in menu mode
  if ( __MODE_MENU && __BLINK ) {
    __BLINK_CHANGE;
    __UPDATE(menuItem);
  }

  // Update 1st line on screen
  if (needUpdateUI == UPDATE_1ST || needUpdateUI == UPDATE_ALL) {
    __SERIAL_LN("Update 1st line");

    lcd.setCursor(0, 0);
    lcd.print("NP:  , PS:     ");

    if (modeUI == MODE_READY || bBlink || menuItem != MENU_PNUM) {
      lcd.setCursor(4, 0);
      lcd.print(menuData[MENU_PNUM]);
    }

    if (modeUI == MODE_READY || bBlink || menuItem != MENU_PSHIFT ) {
      lcd.setCursor(11, 0);
      lcd.print(menuData[MENU_PSHIFT]);
      lcd.print("%");
      // Since pulse duration depends on zero shift, we need update it too
      __UPDATE(MENU_PDURATION);  
    }
  }
  
  // Update 2nd line on screen
  if (needUpdateUI == UPDATE_2ND || needUpdateUI == UPDATE_ALL) {
    __SERIAL_LN("Update 2nd line");

    lcd.setCursor(0, 1);
    lcd.print("PD:             ");
    if (modeUI == MODE_READY || bBlink || menuItem != MENU_PDURATION) {
      lcd.setCursor(4, 1);
      lcd.print(1.0 * menuData[MENU_PDURATION] + 10 - 0.1 * menuData[MENU_PSHIFT]);
      lcd.print(" ms");
    }
  }

  // Reset update flag
  if (needUpdateUI)
    needUpdateUI = UPDATE_NONE;

}  // loop()

void zeroCrossInterrupt() {
  // We have to add ~300us to compensate zero-detection pre-pulse
  // zeroCrossTime = micros() + 300;
  flagZero = true;
}

void eepromLoad() {
  __SERIAL_LN("Load data from EEPROM");
  for (int i = 0; i < MENU_ITEMS; i++)
    EEPROM.get(i * sizeof(uint16_t), menuData[i]);
}
void eepromSave() {
  __SERIAL_LN("Save data from EEPROM");
  for (int i = 0; i < MENU_ITEMS; i++)
    EEPROM.put(i * sizeof(uint16_t), menuData[i]);
}

// Interrupt is called once a millisecond,
SIGNAL(TIMER0_COMPA_vect)
{
  // We add encoder routine to timer0
  encoder->service();

  // Track FIRE button
  if (!flagFire && digitalRead(pinFire) == LOW)
    flagFire = true;
}

