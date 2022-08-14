#define SPI_INTERFACES_COUNT   0

//#include <Arduino.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <DS3231.h>
//#include <DS1307RTC.h>
//#include "uRTCLib.h"
#include <LibPrintf.h>
#include <EEPROM.h>


//DS3232RTC

// piny fizyczne
#define _PB0 8
#define _PB1 9
#define _PB2 10
#define _PB3 11
#define _PB4 12
#define _PB5 13

#define _PD0 0
#define _PD1 1
#define _PD2 2
#define _PD3 3
#define _PD4 4
#define _PD5 5
#define _PD6 6
#define _PD7 7

#define _PC0 14
#define _PC1 15
#define _PC2 16
#define _PC3 17
#define _PC4 18
#define _PC5 19

// piny logiczne
const int PIN_LCD_RS = 8, PIN_LCD_EN = 9, PIN_LCD_D4 = 4, PIN_LCD_D5 = 5, PIN_LCD_D6 = 6, PIN_LCD_D7 = 7; 

const int PIN_LCD_BACK_LIGHT = 10 ; // pin od podświetlenia wyświetlacza
const int PIN_RELAY   = _PC1  ; // pin przekaźnika
const int PIN_LED     = _PB5  ; // pin diody LED
const int PIN_ONEWIRE = _PC0  ; // pin diody LED

const int PIN_BTN_SEL  = _PD4  ; // button SELECT
const int PIN_BTN_UP   = _PD3  ; // button UP
const int PIN_BTN_DN   = _PD2  ; // button DOWN

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

OneWire oneWire;

DallasTemperature sensors;

#define RELAY_ON HIGH
#define RELAY_OFF LOW

#define ISR_FREQ 2 

#define RTC1
#ifdef RTC1
DS3231 rtc;
#endif
#ifdef RTC2
RTC_DS1307 rtc;
#endif

byte minTemp = 45; // 'C
int pumpTime = 30; //second
byte riseTemp = 1; //'C
byte tempCheckInterval = 10; //'C
byte pumpOperationTime[2][4] = {{7,0,7,30}, {9,0,9,30}}; // two timers; startHour,startMinut,endHour,endMinute

bool isPumpWorking  = false;

int lcd_key     = 0;
int adc_key_in  = 0;
int prev_adc_key_in = 0;
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   -1

byte fanImg1[8] = {B11111, B11111, B01110, B00000, B00000, B00000, B00000, B00000};
byte fanImg2[8] = {B11000, B11100, B11100, B11100, B11000, B00000, B00000, B00000};
byte fanImg3[8] = {B00000, B00000, B00000, B00000, B00000, B01110, B11111, B11111};
byte fanImg4[8] = {B00000, B00000, B00000, B00011, B00111, B00111, B00111, B00011};

byte fanRot1[8] = {B11100, B01110, B00110, B00010, B00000, B00000, B00000, B00000};
byte fanRot2[8] = {B00000, B00000, B00000, B00000, B01110, B11100, B11000, B10000};
byte fanRot3[8] = {B00000, B00000, B00000, B00000, B01000, B01100, B01110, B00111};
byte fanRot4[8] = {B00001, B00011, B00111, B01110, B00000, B00000, B00000, B00000};

int read_LCD_buttons()
{
  prev_adc_key_in = adc_key_in;
  adc_key_in = analogRead(0);
  int pressedBtn;

    //if (adc_key_in > 1000) pressedBtn = btnNONE; // 1022
    if (adc_key_in < 50)   pressedBtn = btnRIGHT; // 0
    else if (adc_key_in < 195)  pressedBtn = btnUP; // 99 
    else if (adc_key_in < 380)  pressedBtn = btnDOWN; // 258
    else if (adc_key_in < 500)  pressedBtn = btnLEFT; // 411
    else if (adc_key_in < 700)  pressedBtn = btnSELECT;
    else pressedBtn = btnNONE; // 1022

  if(pressedBtn != btnNONE)  {

    
    while(adc_key_in <= 1000) {
        delay(200);
        adc_key_in = analogRead(0);
        //Serial.println(analogRead(0));
    }
  }

//Serial.println("Pressed: " + String(pressedBtn,DEC) + ", " + String(adc_key_in,DEC));
  return pressedBtn;  // when all others fail, return this...
}

void setupFanImg()
{
  lcd.createChar(0, fanImg1);
  lcd.createChar(1, fanImg2);
  lcd.createChar(2, fanImg3);
  lcd.createChar(3, fanImg4);
  
  lcd.createChar(4, fanRot1);
  lcd.createChar(5, fanRot2);
  lcd.createChar(6, fanRot3);
  lcd.createChar(7, fanRot4);
}

void printFan(bool rotated)
{
  if(rotated)
  {
    lcd.setCursor(8, 1);
    lcd.write(byte(0));
    lcd.setCursor(9, 1);
    lcd.write(byte(1));
    lcd.setCursor(9, 0);
    lcd.write(byte(2));
    lcd.setCursor(8, 0);
    lcd.write(byte(3));
  } 
  else 
  {
    lcd.setCursor(9, 1);
    lcd.write(byte(4));
    lcd.setCursor(9, 0);
    lcd.write(byte(5));
    lcd.setCursor(8, 0);
    lcd.write(byte(6));
    lcd.setCursor(8, 1);
    lcd.write(byte(7));
  }
}

byte testButton(byte pin) {
  if (!digitalRead(pin)) {
    digitalWrite(PIN_LED, LOW );
      while(!digitalRead(pin)) {}
    digitalWrite(PIN_LED, HIGH);
    return 1;
  }
  return 0;
}

void setupOneWire() {
    oneWire = OneWire(PIN_ONEWIRE);
}

void setupPorts() {
  DDRB = 0x00; // Port B input
  DDRC = 0x00; // Port C input
  DDRD = 0x00; // Port D input
}

void setupRelay() {
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, RELAY_OFF);
}

void setupLCD() {
  pinMode(PIN_LCD_BACK_LIGHT, OUTPUT);
  digitalWrite(PIN_LCD_BACK_LIGHT, HIGH);
  lcd.begin(16, 2);
}

void setupLED() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
}

void errorLoop(String msg) {
    lcd.clear();
    lcd.print(msg);
    lcd.setCursor(0,1);
    lcd.print("Error!");
    while(1) {
        digitalWrite(PIN_LED, LOW );
        delay(1000);
        if (testButton(PIN_BTN_SEL)) return;
        digitalWrite(PIN_LED, HIGH);
        delay(1000);
        if (testButton(PIN_BTN_SEL)) return;
    }
}

void msgOK(String msg) {
    lcd.clear();
    lcd.print(msg);
    lcd.setCursor(0,1);
    lcd.print("OK");
    delay(250);
}

void setupRTC() {
    Wire.begin();
    rtc.setClockMode(false);
    #ifdef RTC2
    if (rtc.isrunning() )
    #endif
    #ifdef RTC1
    float temp = rtc.getTemperature();
    if (temp != 0)
    #endif
        msgOK("RTC");
    else
        errorLoop("RTC");
}

void setupSensor() {
    sensors = DallasTemperature(&oneWire);
    sensors.begin(); 
    sensors.requestTemperatures();
    lcd.clear();
    lcd.print("Sensors:");
    
    byte n =  sensors.getDeviceCount();
    lcd.print(n);
    if (n == 1)
        msgOK("Sensor");
    else 
        errorLoop("Sensor");
} 

void setupButtons() {
    pinMode(PIN_BTN_SEL, INPUT);
    digitalWrite(PIN_BTN_SEL, HIGH);
    pinMode(PIN_BTN_UP, INPUT);
    digitalWrite(PIN_BTN_UP, HIGH);
    pinMode(PIN_BTN_DN, INPUT);
    digitalWrite(PIN_BTN_DN, HIGH);
}

//float timerValue = 0;                   //Preload timer value (3035 for 4 seconds)
int timerCount =  0;
#define ISR_TIMER_CNT (0xffff-(14962/ISR_FREQ))

void setupTimer() {
  timerCount = tempCheckInterval * ISR_FREQ;
  noInterrupts();                       // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = ISR_TIMER_CNT;                        // preload timer
  TCCR1B |= (1 << CS10)|(1 << CS12);    // 1024 prescaler 
  TIMSK1 |= (1 << TOIE1);               // enable timer overflow interrupt ISR
  interrupts();                         // enable all interrupts
}


void getValueFromEEPROM(int idx, byte* val) {
    byte tmp = EEPROM.read(idx);
    if(tmp != 255)
        *val = tmp;
}

/**
 * Populates variables from EEPROM memory
 * 1st byte: minTemp
 * 2nd byte: riseTemp
 * 3rd byte: tempCheckInterval
 * 4th byte: pumpOperationTime1 - startHour
 * 5th byte: pumpOperationTime1 - startMinute
 * 6th byte: pumpOperationTime1 - endHour
 * 7th byte: pumpOperationTime1 - endMinute
 * 8th byte: pumpOperationTime2 - startHour
 * 9th byte: pumpOperationTime2 - startMinute
 * 10th byte: pumpOperationTime2 - endHour
 * 11th byte: pumpOperationTime2 - endMinute
**/
void getFromEEPROM() {
    getValueFromEEPROM(0, &minTemp);
    getValueFromEEPROM(1, &riseTemp);
    getValueFromEEPROM(2, &tempCheckInterval);
    getValueFromEEPROM(3, &(pumpOperationTime[0][0]));
    getValueFromEEPROM(4, &(pumpOperationTime[0][1]));
    getValueFromEEPROM(5, &(pumpOperationTime[0][2]));
    getValueFromEEPROM(6, &(pumpOperationTime[0][3]));
    getValueFromEEPROM(7, &(pumpOperationTime[1][0]));
    getValueFromEEPROM(8, &(pumpOperationTime[1][1]));
    getValueFromEEPROM(9, &(pumpOperationTime[1][2]));
    getValueFromEEPROM(10, &(pumpOperationTime[1][3]));
}



void setup() {
  Serial.begin(9600);
  setupPorts();
  setupRelay();
  //setupButtons();
  setupLCD();
  setupLED();
  setupOneWire();
  setupSensor();
  setupRTC();
  getFromEEPROM();
  setupTimer();
  setupFanImg();
}

String getDigit(int digit)
{
  if (digit < 10)
    return '0' + String(digit, DEC);
  return String(digit, DEC);
}

bool century = false;
bool h12Flag = false;
bool pmFlag;
//RTCLib
void printCurrentTime() {
	lcd.print(getDigit(rtc.getHour(h12Flag, pmFlag)));
	lcd.print(":");
	lcd.print(getDigit(rtc.getMinute()));
	lcd.print(":");
	lcd.print(getDigit(rtc.getSecond()));
    lcd.print("  ");

    lcd.print(getDigit(rtc.getDate()));
	lcd.print('/');
	lcd.print(getDigit(rtc.getMonth(century)));
}


float printTemperature() {
    sensors.requestTemperaturesByIndex(0);
    float temp = sensors.getTempCByIndex(0);
    lcd.print(temp);
    lcd.print((char)223);
    lcd.print("C ");
    return temp;
}

/**
 * Returns true if current time is "inside" the time specified in the timers when the pump should run
**/
bool insidePumpOperationTime() {
    int timerStartTime, timerStopTime;
    byte currHour = rtc.getHour(h12Flag, pmFlag);
    byte currMinute = rtc.getMinute();
    int currTime = currHour * 60 + currMinute;
    //Serial.println("currTime:" + String(currTime,DEC) + ",currHour:" + String(currHour,DEC) + ",currMinute:" + String(currMinute,DEC));
    for(int i = 0; i < 2; i++) {
        timerStartTime = pumpOperationTime[i][0] * 60 + pumpOperationTime[i][1];
        timerStopTime = pumpOperationTime[i][2] * 60 + pumpOperationTime[i][3];
        if(timerStartTime == timerStopTime) continue;
        if(timerStartTime < timerStopTime) {    // i.e. from 23:00 to 05:00
            if(currTime > timerStartTime && currTime < timerStopTime)
                return true;
        }
        else if(timerStartTime > timerStopTime) { // i.e. from 07:00 to 08:00
            if(currTime > timerStartTime || currTime < timerStopTime)
                return true;
        }
    }
    return false;
}

#define MAX_TEMP 999
float prevTemp  = MAX_TEMP;

ISR(TIMER1_OVF_vect)                   
{
  
  noInterrupts(); 
  TCNT1  = ISR_TIMER_CNT;
  digitalWrite(PIN_LED, digitalRead(PIN_LED) ^ 1);  // LED blink

  if (digitalRead(PIN_RELAY) == RELAY_ON) {
    //Serial.println("is ON!, timerCount: " + String(timerCount, DEC));
    timerCount--;
    if(timerCount == 0) {
        prevTemp = sensors.getTempCByIndex(0);
        digitalWrite(PIN_RELAY, RELAY_OFF);
        isPumpWorking = false;
        timerCount = tempCheckInterval * ISR_FREQ;
    }
  } else {
      float temp = sensors.getTempCByIndex(0);
      //Serial.println("is OFF!, timerCount: " + String(timerCount, DEC) + ", delta=" + String(temp - prevTemp, DEC));
      timerCount--;
      
      if(timerCount == 0 || prevTemp == MAX_TEMP) {
          prevTemp = temp;
          timerCount = tempCheckInterval * ISR_FREQ;
      } else {
          //bool test = insidePumpOperationTime();
          //Serial.print("insidePumpOperationTime:");
          //if(test) Serial.print("true");
          //else Serial.print("false");

        if ( insidePumpOperationTime() ||  ((temp - prevTemp) > riseTemp && temp <= minTemp) ) {
            digitalWrite(PIN_RELAY, RELAY_ON);
            isPumpWorking = true;
            timerCount = pumpTime * ISR_FREQ;
        }
      }
  }
  interrupts(); 
}

#define SCENE_MAIN 1
#define SCENE_SET_MIN_TEMP 2
#define SCENE_SET_RISE_TEMP 3
#define SCENE_SET_TEMP_CHECK_INT 4
#define SCENE_SET_OPERATION_TIME 5
#define SCENE_SET_TIME 6

byte sceneMain () {
    Serial.print("\nbegin sceneMain: ");
    lcd.clear();
    int tmp = 1000;
    bool fanRot = false;
    while(1) {
        tmp = tmp - 1;
        if(tmp <= 0) {
            tmp = 1000;
            lcd.setCursor(0, 0);
            printCurrentTime();

            if(isPumpWorking)
                fanRot ^= true;
            printFan(fanRot);

            lcd.setCursor(0, 1);
            printTemperature();
        }
        int btnPressed = read_LCD_buttons();
        //Serial.println("btnPressed: " + String(btnPressed,DEC));
        //delay(500);
        switch (btnPressed)
        {
        case btnRIGHT:
            {
            Serial.print("sceneMain: btnRIGHT; ");
                return SCENE_SET_MIN_TEMP;
            }
        case btnLEFT:
            {
                Serial.print("sceneMain: btnLEFT; ");
                return SCENE_SET_TIME;
            }
        }
    }
}

int8_t sceneSetMinTemp () {
    Serial.print("\nbegin sceneSetMinTemp: ");
    lcd.clear();
    lcd.print("Set min temp: ");
    while(1) {
        int btnPressed = read_LCD_buttons();
        switch (btnPressed)
        {
        case btnRIGHT:
            {
            return SCENE_SET_RISE_TEMP;
            }
        case btnLEFT:
            {
            return SCENE_MAIN;
            }
        case btnUP:
            {
            minTemp++;
            break;
            }
        case btnDOWN:
            {
            minTemp--;
            break;
            }
        case btnSELECT:
        {
            // save minTemp if changed
            EEPROM.update(0, minTemp);
            delay(500);
            break;
        }
        }
        lcd.setCursor(0, 1); 
        lcd.print(minTemp);
        lcd.print((char)223);
        lcd.print("C");
    }
}

int8_t sceneSetRiseTemp () {
    Serial.print("\nbegin scene SetRiseTemp: ");
    lcd.clear();
    lcd.print("Set rise temp:");
    while(1) {
        int btnPressed = read_LCD_buttons();
        switch (btnPressed)
        {
        case btnRIGHT:
            {
            return SCENE_SET_TEMP_CHECK_INT;
            }
        case btnLEFT:
            {
            return SCENE_SET_MIN_TEMP;
            }
        case btnUP:
            {
            riseTemp++;
            break;
            }
        case btnDOWN:
            {
            riseTemp--;
            break;
            }
        case btnSELECT:
        {
            // save riseTemp if changed
            EEPROM.update(1, riseTemp);
            delay(500);
            break;
        }
        }
        lcd.setCursor(0, 1);
        lcd.print(String(riseTemp,DEC));
        lcd.print((char)223);
        lcd.print("C");
    }
}

byte sceneSetTempCheckInterval () {
    Serial.print("\nbegin sceneSetTempCheckInterval: ");
    lcd.clear();
    lcd.print("Set temp check int:");
    while(1) {
        int btnPressed = read_LCD_buttons();
        switch (btnPressed)
        {
        case btnRIGHT:
            {
            return SCENE_SET_OPERATION_TIME;
            }
        case btnLEFT:
            {
            return SCENE_SET_RISE_TEMP;
            }
        case btnUP:
            {
            tempCheckInterval++;
            break;
            }
        case btnDOWN:
            {
            tempCheckInterval--;
            break;
            }
        case btnSELECT:
        {
            // save tempCheckInterval if changed
            EEPROM.update(2, tempCheckInterval);
            delay(500);
            break;
        }
        }
        lcd.setCursor(0, 1);
        lcd.print(String(tempCheckInterval, DEC) + " sec  ");
    }
}

int getTimeCursorPosition(int position) {
    switch (position) {
        case 0: return 0;
        case 1: return 3;
        case 2: return 6;
        case 3: return 10;
        case 4: return 13;
    }
    return 0;
}

byte setTimeFromPosition(int position, bool up) {
    byte val = 0;
    int delta = (up) ? 1 : -1;
    switch (position) {
        case 0: 
            val = rtc.getHour(h12Flag, pmFlag) + delta;
            rtc.setHour(val);
            break;
        case 1: 
            val = rtc.getMinute() + delta;
            rtc.setMinute(val);
            break;
        case 2: 
            val = rtc.getSecond() + delta;
            rtc.setSecond(val);
            break;
        case 3: 
            val = rtc.getDate() + delta;
            rtc.setDate(val);
            break;
        case 4: 
            val = rtc.getMonth(century) + delta;
            rtc.setMonth(val);
            break;
    }
    return val;
}

byte getTimeFromPosition(int position) {
    switch (position) {
        case 0: return rtc.getHour(h12Flag, pmFlag);
        case 1: return rtc.getMinute();
        case 2: return rtc.getSecond();
        case 3: return rtc.getDate();
        case 4: return rtc.getMonth(century);
    }
    return 0;
}

/**
 * Sets time (minute/hour) when the pump should run
 * There are 4 positions: startHour 0 (8,0) , startMinute 1 (11,0), endHour 2 (8,1), endMinute 3 (11,3) 
**/
byte sceneSetPumpOperatingTime () {
    char line[16];
    Serial.print("\nbegin sceneSetPumpOperatingTime: ");
    lcd.clear();
    byte timerId = 0; // for now, there are 2 timers - 0 and 1
    bool isSelected = false;
    int position = 0;
    while(1) {
        lcd.setCursor(0, 1);
        int btnPressed = read_LCD_buttons();
        switch (btnPressed)
        {
        case btnRIGHT:
            {
            Serial.print("right; ");
            if(!isSelected) {
                if(timerId == 1)
                    return SCENE_SET_TIME;
                timerId = 1;
                break;
            }
            if(++position >= 4) position = 0;
            Serial.println("position=  " + String(position) + ";");
            break;
            }
        case btnLEFT:
            {
            Serial.print("left; ");
            if(!isSelected) {
                if(timerId == 0)
                    return SCENE_SET_TEMP_CHECK_INT;
                timerId = 0;
                break;
            } 
            if(--position <= -1) position = 4;
            Serial.println("position=  " + String(position) + ";");
            break;
            }
        case btnUP:
            {
            Serial.print("up; ");
            if(isSelected)
            {
                if(position == 0 || position == 2) {
                    // handle hour change
                    if(pumpOperationTime[timerId][position] == 23)
                        pumpOperationTime[timerId][position] = 0;
                    else
                        ++pumpOperationTime[timerId][position];         
                }
                else if(position == 1 || position == 3) {
                    // handle minute change
                    if(pumpOperationTime[timerId][position] == 55)
                        pumpOperationTime[timerId][position] = 0;
                    else
                        pumpOperationTime[timerId][position] += 5;
                } 
                Serial.println("position=" + String(position) + "," + String(pumpOperationTime[timerId][position]));
            }
            break;
            }
        case btnDOWN:
            {
            Serial.print("down; ");
            if(isSelected)
            {
                if(position == 0 || position == 2) {
                    // handle hour change
                    if(pumpOperationTime[timerId][position] == 0)
                        pumpOperationTime[timerId][position] = 23;
                    else
                        --pumpOperationTime[timerId][position];         
                }
                else if(position == 1 || position == 3) {
                    // handle minute change
                    if(pumpOperationTime[timerId][position] == 0)
                        pumpOperationTime[timerId][position] = 55;
                    else
                        pumpOperationTime[timerId][position] -= 5;
                } 
                Serial.println("position=" + String(position)  + "," + String(pumpOperationTime[timerId][position]));
            }
            break;
            }
        case btnSELECT:
            {
                Serial.print("select; ");
                if(isSelected) {
                    isSelected = false;
                    // save variables
                    Serial.println("Saving pumpOperationTime variables...");
                    if(timerId == 0) {
                        EEPROM.update(3, pumpOperationTime[0][0]);
                        EEPROM.update(4, pumpOperationTime[0][1]);
                        EEPROM.update(5, pumpOperationTime[0][2]);
                        EEPROM.update(6, pumpOperationTime[0][3]);
                    }
                    else if(timerId == 1) {
                        EEPROM.update(7, pumpOperationTime[1][0]);
                        EEPROM.update(8, pumpOperationTime[1][1]);
                        EEPROM.update(9, pumpOperationTime[1][2]);
                        EEPROM.update(10, pumpOperationTime[1][3]);
                    }
                    delay(500);
                } else {
                    isSelected = true;
                }
            break;
            }
        }
        
        lcd.noBlink();
        lcd.noCursor();
        lcd.setCursor(0, 0);
        sprintf(line, "%.1dstart: %.2d:%.2d   ", timerId+1, pumpOperationTime[timerId][0], pumpOperationTime[timerId][1]);
        lcd.print(line);
        lcd.setCursor(0, 1);
        sprintf(line, "%.1dend:   %.2d:%.2d   ", timerId+1, pumpOperationTime[timerId][2], pumpOperationTime[timerId][3]);
        lcd.print(line);
        if(isSelected) {
            lcd.setCursor(0, 1);
            lcd.blink();
            lcd.cursor();
            delay(300);
        } 
    }
}

byte sceneSetCurrentTime () {
    Serial.print("\nbegin sceneSetCurrentTime: ");
    lcd.clear();
    lcd.print("Set time:");
    bool isSelected = false;
    int position = 0;
    int cursorPos = 3;
    while(1) {
        lcd.setCursor(0, 1);
        int btnPressed = read_LCD_buttons();
        switch (btnPressed)
        {
        case btnRIGHT:
            {
            Serial.print("right; ");
            if(!isSelected) return SCENE_MAIN;
            if(++position >= 5) position = 0;
            cursorPos = getTimeCursorPosition(position);
            Serial.print("cursorPos=  " + String(cursorPos) + ";");
            break;
            }
        case btnLEFT:
            {
            Serial.print("left; ");
            if(!isSelected) return SCENE_SET_OPERATION_TIME;
            if(--position <= -1) position = 4;
            cursorPos = getTimeCursorPosition(position);
            Serial.print("cursorPos=  " + String(cursorPos) + ";");
            break;
            }
        case btnUP:
            {
            Serial.print("up; ");
            if(isSelected)
            {
                Serial.print("position=  " + String(position) + ";");
                setTimeFromPosition(position, true);
            }
            break;
            }
        case btnDOWN:
            {
            Serial.print("down; ");
            if(isSelected)
            {
                Serial.print("position=  " + String(position) + ";");
                setTimeFromPosition(position, false);
            }
            break;
            }
        case btnSELECT:
            {
                Serial.print("select; ");
                if(isSelected) {
                    isSelected = false;
                } else {
                    isSelected = true;
                    lcd.noBlink();
                    lcd.noCursor();
                }
            break;
            }
        }
        
        lcd.noBlink();
        lcd.noCursor();
        lcd.setCursor(0, 1);
        printCurrentTime();
        if(isSelected) {
            lcd.setCursor(cursorPos, 1);
            lcd.blink();
            lcd.cursor();
            delay(300);
        } 
    }
}


byte scene = SCENE_MAIN;
#define MAX_SCENE 5

void loop() {
    
  switch(scene) {
      case SCENE_MAIN: scene = sceneMain(); break;
      case SCENE_SET_MIN_TEMP: scene = sceneSetMinTemp(); break;
      case SCENE_SET_RISE_TEMP: scene = sceneSetRiseTemp(); break;
      case SCENE_SET_TEMP_CHECK_INT: scene = sceneSetTempCheckInterval(); break;
      case SCENE_SET_OPERATION_TIME: scene = sceneSetPumpOperatingTime(); break;
      case SCENE_SET_TIME: scene = sceneSetCurrentTime(); break;
  }
}