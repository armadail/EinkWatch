#include <Wire.h>
#include <avr/sleep.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include "RTClib.h"

// --- NEW GLOBALS FOR UI ---
#define PIN_SELECT 3  // Button 1: Cycle through elements
#define PIN_ADJUST 4  // Button 2: Increment value
#define PIN_DEBUG 6
// --- PROGMEM FIXED: Explicit array dimensions [Items][Length] ---
const char monthNames[][4] PROGMEM = {
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

const char daysOfTheWeek[][4] PROGMEM = {
  "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

// 1.54" E-ink Display (CS, DC, RST, BUSY)
GxEPD2_BW<GxEPD2_154_D67, 32> display(GxEPD2_154_D67(10, 9, 8, 7));
RTC_PCF8563 rtc;

enum EditState { NORMAL, SET_DOW, SET_MONTH, SET_DAY, SET_YEAR, SET_H_TENS, SET_H_ONES, SET_M_TENS, SET_M_ONES };
EditState currentEditMode = NORMAL;


// Temporary variables to hold settings during edit
int t_dow, t_month, t_day, t_year, t_hour, t_min;

volatile bool RTC_Triggered = false;
bool isDarkMode = true; 

// Calibrated internal voltage check
float getBatteryVoltage() {
  
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); 
  ADCSRA |= _BV(ADSC); 
  while (bit_is_set(ADCSRA, ADSC)); 
  uint8_t low = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  return 1400.0 / result; 
}

void wakeUpISR() {
  RTC_Triggered = true;
}

void writeRTC(byte reg, byte data) {
  Wire.beginTransmission(0x51);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();

  // endTransmission returns a status code:
  // 0: success
  // 1: data too long to fit in transmit buffer
  // 2: received NACK on transmit of address
  // 3: received NACK on transmit of data
  // 4: other error

  byte error = Wire.endTransmission();
  
  if (error != 0) {
    Serial.print(F("I2C Error: "));
    Serial.println(error);
  }
}

void initRTC() {
  writeRTC(0x00, 0x00); 
  writeRTC(0x01, 0x01); // Enable Timer interrupt
  writeRTC(0x0E, 0x82); // 1Hz source
  writeRTC(0x0F, 0x3C); // 60 Seconds countdown
}

void clearRTCInterrupt() {
  writeRTC(0x01, 0x01); 
}

void handleSelectPress() {

  Serial.println(F("Entering handleSelectPress..."));
  
  if (currentEditMode == NORMAL) {
    //Force a Wire restart to clear any potential hangs from the E-ink refresh
    Wire.end(); 
    delay(10);
    Wire.begin();
    // Try to get time, but don't die if it fails
    DateTime now = rtc.now(); 
    
    // Check if the RTC actually returned a valid year
    // Fallback if I2C still fails
    if (now.year() < 2020 || now.year() > 2100) {
       Serial.println(F("I2C Bus Glitch: Using 2026 Manual Settings"));
       t_year = 2026; t_month = 3; t_day = 28; t_hour = 12; t_min = 0; t_dow = 6;
    } else {
       t_dow = now.dayOfTheWeek(); t_month = now.month(); t_day = now.day();
       t_year = now.year(); t_hour = now.hour(); t_min = now.minute();
    }
    currentEditMode = SET_DOW;
    // Wire.beginTransmission(0x51);
    // if (Wire.endTransmission() != 0) {
    //   // BUS IS DEAD - DO NOT CALL rtc.now()
    //   Serial.println(F("RTC not found! Using defaults."));
    //   t_year = 2026; t_month = 3; t_day = 28; // etc...
    // } else {
    //   // BUS IS OK
    //   DateTime now = rtc.now();
    //   t_year = now.year(); // etc...
    // }
    // currentEditMode = SET_DOW;
  }
  else if (currentEditMode == SET_M_ONES) {
    // Finished last element: Save to RTC and exit
    rtc.adjust(DateTime(t_year, t_month, t_day, t_hour, t_min, 0));
    currentEditMode = NORMAL;
  } else {
    // Move to next element
    currentEditMode = (EditState)((int)currentEditMode + 1);
  }
}

void handleAdjustPress() {
  switch (currentEditMode) {
    case SET_DOW:    t_dow = (t_dow + 1) % 7; break;
    case SET_MONTH:  t_month = (t_month % 12) + 1; break;
    case SET_DAY:    t_day = (t_day % 31) + 1; break;
    case SET_YEAR:   t_year = (t_year >= 2030) ? 2026 : t_year + 1; break;
    case SET_H_TENS: t_hour = (t_hour >= 20) ? t_hour - 20 : t_hour + 10; if(t_hour > 23) t_hour = 20; break;
    case SET_H_ONES: t_hour = ((t_hour/10 == 2) && (t_hour%10 == 3)) ? 20 : (t_hour%10 == 9) ? (t_hour/10)*10 : t_hour + 1; break;
    case SET_M_TENS: t_min = (t_min >= 50) ? t_min - 50 : t_min + 10; break;
    case SET_M_ONES: t_min = (t_min % 10 == 9) ? (t_min/10)*10 : t_min + 1; break;
  }
}

void renderWatchFace(DateTime now, float batt, bool darkMode) {
  uint16_t bgColor = darkMode ? GxEPD_BLACK : GxEPD_WHITE;
  uint16_t fgColor = darkMode ? GxEPD_WHITE : GxEPD_BLACK;

  display.firstPage();
  do {
    display.fillScreen(bgColor); 
    display.setTextColor(fgColor); 
    
    // --- 1. DATE LINE (TOP) ---
    display.setTextSize(2);
    display.setCursor(15, 30);
    
    char dayBuf[4];
    char monthBuf[4];

    strcpy_P(dayBuf, daysOfTheWeek[now.dayOfTheWeek()]);
    strcpy_P(monthBuf, monthNames[now.month()-1]);
    
    display.print(dayBuf); display.print(F(" "));
    display.print(monthBuf); display.print(F(" "));
    display.print(now.day()); display.print(F(" "));
    display.print(now.year());

    // --- 2. TIME LINE (CENTER) ---
    display.setCursor(15, 100); 
    display.setTextSize(5);   
    if(now.hour() < 10) display.print('0');
    display.print(now.hour());
    display.print(':');
    if(now.minute() < 10) display.print('0');
    display.print(now.minute());
    
    // --- 3. BATTERY LINE (BOTTOM) ---
    display.setTextSize(1);
    display.setCursor(80, 165);
    display.print(batt);
    display.print(F("V"));
    
    // --- 4. EDIT CURSOR ---
    if (currentEditMode != NORMAL) {
      switch(currentEditMode) {
        case SET_DOW:    display.drawFastHLine(15, 45, 35, fgColor); break;
        case SET_MONTH:  display.drawFastHLine(63, 45, 35, fgColor); break; 
        case SET_DAY:    display.drawFastHLine(111, 45, 20, fgColor); break;
        case SET_YEAR:   display.drawFastHLine(145, 45, 50, fgColor); break; 
        case SET_H_TENS: display.drawFastHLine(15, 140, 25, fgColor); break;
        case SET_H_ONES: display.drawFastHLine(45, 140, 25, fgColor); break;
        case SET_M_TENS: display.drawFastHLine(105, 140, 25, fgColor); break;
        case SET_M_ONES: display.drawFastHLine(135, 140, 25, fgColor); break;
        default: break;
      }
    }
  } while (display.nextPage());
}

void updateDisplayEdit(DateTime now, float batt, bool darkMode) {
  // Only proceed if the display is actually ready
  // If it's still busy from the last click, skip this frame to prevent a freeze
  if (digitalRead(7) == HIGH) {
    return; 
  }

  display.init(115200, false); 
  display.setPartialWindow(0, 0, display.width(), display.height());
  
  renderWatchFace(now, batt, darkMode);
  
}

void updateDisplaySleep(DateTime now, float batt, bool darkMode) {
  // Hard Hardware Reset for stability before long sleep
  digitalWrite(8, LOW); delay(10); digitalWrite(8, HIGH); delay(10);
  
  display.init(115200, false);
  display.setPartialWindow(0, 0, display.width(), display.height());
  
  renderWatchFace(now, batt, darkMode);
  
  display.powerOff(); // Deep sleep for the E-ink controller
}


void setup() {
  Serial.begin(115200);
  delay(500); 

  pinMode(PIN_DEBUG, OUTPUT); 
  pinMode(2, INPUT_PULLUP);  // RTC interrupt
  pinMode(PIN_SELECT, INPUT_PULLUP);
  pinMode(PIN_ADJUST, INPUT_PULLUP);

  Wire.begin();
  Wire.setClock(400000); 

  rtc.begin();
  
  initRTC(); 

  display.init(115200, true, 2, false); 
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0)); //8Mhz -> 4Mhz = max transmision, 2Mhz = stability
  display.setRotation(3);

// Attach BOTH interrupts to wake the watch
  attachInterrupt(digitalPinToInterrupt(2), wakeUpISR, LOW); // RTC Wakeup
  attachInterrupt(digitalPinToInterrupt(3), wakeUpISR, LOW); // Select Button Wakeup

  //initial refresh during boot
  updateDisplaySleep(rtc.now(), getBatteryVoltage(), isDarkMode);
}

void loop() {

  // 1. Logic Gate: SELECT
  if (digitalRead(PIN_SELECT) == LOW) { 
    handleSelectPress(); 
    delay(50); //debounce
    while(digitalRead(PIN_SELECT) == LOW); 
    DateTime displayTime = (currentEditMode == NORMAL) ? rtc.now() : DateTime(t_year, t_month, t_day, t_hour, t_min, 0);
    updateDisplayEdit(displayTime, getBatteryVoltage(), isDarkMode); 
  }

  // 2. Logic Gate: ADJUST
  if (currentEditMode != NORMAL && digitalRead(PIN_ADJUST) == LOW) { 
    handleAdjustPress();
    delay(50); //debounce
    while(digitalRead(PIN_ADJUST) == LOW);

    updateDisplayEdit(DateTime(t_year, t_month, t_day, t_hour, t_min, 0), getBatteryVoltage(), isDarkMode); 
  }


  // 4. Normal RTC Wakeup

  if (RTC_Triggered && currentEditMode == NORMAL) {
    RTC_Triggered = false;
    detachInterrupt(digitalPinToInterrupt(2));
    clearRTCInterrupt();
    isDarkMode = !isDarkMode;
    updateDisplaySleep(rtc.now(), getBatteryVoltage(), isDarkMode);
    attachInterrupt(digitalPinToInterrupt(2), wakeUpISR, LOW);
  }

  if (currentEditMode == NORMAL && digitalRead(PIN_SELECT) == HIGH && digitalRead(PIN_ADJUST) == HIGH) {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode(); // Use sleep_mode() as it handles enable/disable safely
  }
}