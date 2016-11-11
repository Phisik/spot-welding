#include <EEPROM.h>
#include <LiquidCrystal.h>
#include "ClickEncoder.h"

//#define __DEBUG

// All our difinitions and global variables are in welding.h
#include "welding.h"

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
  pinMode(pinBrightness, OUTPUT);
  analogWrite(pinBrightness, brightness);
  
  lcd.setCursor(0, 0);   lcd.print("  Spot Welding");
  lcd.setCursor(0, 1);   lcd.print("  2016 (C) LOM");
  delay(ONE_SECOND);

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
    // We will manage every half-wave, so let's count how many half-waves will be
    const uint32_t nHalfWaveNum = menuData[MENU_PDURATION]/(0.001*zeroCrossPeriod);
    __SERIAL("Half Wave number is ");
    __SERIAL_LN(nHalfWaveNum);

    // Pulse power is inversely propotional to the moment of triac firing
    // NB! We do not account for delay between zero-cross interrupt and real zero crossing
    const int fireDelay = zeroCrossPeriod - 100 * user2power(menuData[MENU_POWER]); 
    __SERIAL("Fire delay is ");
    __SERIAL_LN(fireDelay);
      
    // Pause between pulses will be equal to pulse time but not less than 100ms and not more that 1s
    const int pause = min(max(menuData[MENU_PDURATION], 100), ONE_SECOND);  
    
    __FAN_START;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ignite in 3 sec ");
    delay(ONE_SECOND);
    lcd.setCursor(0, 0);
    lcd.print("Ignite in 2 sec ");
    delay(ONE_SECOND);
    lcd.setCursor(0, 0);
    lcd.print("Ignite in 1 sec ");
    delay(ONE_SECOND);

    for (int i = 0; i < menuData[MENU_PNUM]; i++) {
      if (i>0) delay(pause);
      
      lcd.setCursor(0, 0);
      lcd.print("Ignition #      ");
      lcd.setCursor(10, 0);
      lcd.print(i + 1);

      // Ignite first half wave here to avoid any computations inside fire loop
      wait4ZeroCross();
      delayMicroseconds(zeroCrossDelay);
      delayMicroseconds((fireDelay<zeroCrossHalfPeriod)?fireDelay:zeroCrossHalfPeriod); 
      __TRIAC_ON;

      // Fire loop, minimum code here to efficiency
      for(int j = 1; j < nHalfWaveNum; j++){
        // First of all we wait for zero crossing and turn off triac
        wait4ZeroCross();
        __TRIAC_OFF;
        
        // Now we wait for ignition moment and ...
        delayMicroseconds(zeroCrossDelay); 
        delayMicroseconds(fireDelay); 
        
        // Fire! :)
        __TRIAC_ON;
      }

      // Don't forget to switch the triac off when everything is done
      wait4ZeroCross();
      __TRIAC_OFF;
    }

    lcd.setCursor(0, 0);
    lcd.print("Done!           ");
    delay(ONE_SECOND);

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

  if (encoderIncrement && __MODE_READY) {
    brightness += encoderIncrement;
    if (brightness>255) brightness = 255;
    if (brightness<5) brightness = 5;

    analogWrite(pinBrightness, brightness);
    EEPROM.put(MENU_ITEMS*sizeof(uint16_t), brightness);
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
          menuItem = MENU_PDURATION;
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
    lcd.print("NP:  , PW:     ");

    if (modeUI == MODE_READY || bBlink || menuItem != MENU_PNUM) {
      lcd.setCursor(4, 0);
      lcd.print(menuData[MENU_PNUM]);
    }

    if (modeUI == MODE_READY || bBlink || menuItem != MENU_POWER ) {
      lcd.setCursor(11, 0);
      lcd.print(menuData[MENU_POWER]);
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
      lcd.print(1.0 * menuData[MENU_PDURATION]); // + 10 - 0.1 * menuData[MENU_POWER]);
      lcd.print(" ms");
    }
  }

  // Reset update flag
  if (needUpdateUI)
    needUpdateUI = UPDATE_NONE;

}  // loop()

void zeroCrossInterrupt() {
  // We have to add ~300us to compensate zero-detection pre-pulse
  // zeroCrossTime = micros() + zeroCrossDelay;
  flagZero = true;
  wavePlus = !wavePlus;
}

void eepromLoad() {
  __SERIAL_LN("Load data from EEPROM");
  for (int i = 0; i < MENU_ITEMS; i++)
    EEPROM.get(i * sizeof(uint16_t), menuData[i]);
    
  EEPROM.get(MENU_ITEMS*sizeof(uint16_t), brightness);
}
void eepromSave() {
  __SERIAL_LN("Save data from EEPROM");
  for (int i = 0; i < MENU_ITEMS; i++)
    EEPROM.put(i * sizeof(uint16_t), menuData[i]);
    
  EEPROM.put(MENU_ITEMS*sizeof(uint16_t), brightness);
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

// Due to simistor switch off lag we rescale user power to somewhat more reasonable
// User value shold be in %
inline int user2power(int userValue){
    if(userValue>99) 
      return 100;    // full power mode
    else
      return 0.9*userValue;
}



