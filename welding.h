
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
#define MENU_POWER     1
#define MENU_PDURATION  2
#define MENU_PSHIFT     3

int16_t menuData[MENU_ITEMS] = {1, 50, 1000};
const int16_t menuDataMax[MENU_ITEMS] = {9, 100, 30000};
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


// Timers functions
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

const uint8_t pinBrightness = 10;
int16_t brightness = 200;




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


// Event flags
volatile bool flagFire = false;
volatile bool flagZero = false;


// Zero-cross detector config
int zeroCrossDelay  = 300;       // ms
int zeroCrossPeriod = 10000;     // us
int zeroCrossHalfPeriod = 5000;     // us
volatile bool wavePlus = true;
#define wait4ZeroCross() {flagZero = false; while (!flagZero);} // empty cycle


#define __TRIAC_ON   {digitalWrite(pinWeld, HIGH);}
#define __TRIAC_OFF  {digitalWrite(pinWeld, LOW);}

// Use timeout before turning fan off after welding
#define __FAN_START      { flagFan=true;  digitalWrite(pinFan, HIGH); }
#define __FAN_STOP       { flagFan=false; digitalWrite(pinFan, LOW);  }
#define __FAN_DELAY      __TIMEOUT(3, 10000)
#define __FAN_TIMEOUT    __TCHECK(3)
bool flagFan = false;

#define ONE_SECOND 1000




