#include <Wire.h>
#include <avr/sleep.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include "RTClib.h"

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

volatile bool RTC_Triggered = false;
bool isDarkMode = true; 

// Calibrated internal voltage check
float getBatteryVoltage() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); 
  ADCSRA |= _BV(ADSC); 
  while (bit_is_set(ADCSRA, ADSC)); 
  uint8_t low  = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  float vcc = 1400000L / result; 
  return vcc / 1000.0; 
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println(F("\n--- VANCOUVER WATCH: GOLD MASTER ---"));

  pinMode(6, OUTPUT); 
  pinMode(2, INPUT_PULLUP); 

  Wire.begin();
  Wire.setClock(10000); 
  rtc.begin();
  
  // --- ONE-TIME TIME SYNC ---
  // MARCH 27, 2026 @ 14:35:00
  // Upload once, then comment out this line and re-upload!
  // rtc.adjust(DateTime(2026, 3, 28, 16, 35, 0)); 
  
  initRTC(); 

  display.init(115200, true, 2, false); 
  display.setRotation(3);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setTextSize(2);
    display.setCursor(30, 90);
    display.print(F("BOOTING..."));
  } while (display.nextPage());
  display.powerOff();

  attachInterrupt(digitalPinToInterrupt(2), wakeUpISR, LOW);
}

void loop() {
  if (RTC_Triggered) {
    detachInterrupt(digitalPinToInterrupt(2)); 
    digitalWrite(6, HIGH); 
    
    clearRTCInterrupt(); 
    
    DateTime now = rtc.now();
    float batt = getBatteryVoltage();
    isDarkMode = !isDarkMode; // Heartbeat toggle
    
    updateDisplay(now, batt, isDarkMode);
    
    RTC_Triggered = false;
    digitalWrite(6, LOW);
    
    attachInterrupt(digitalPinToInterrupt(2), wakeUpISR, LOW);
  }

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu(); 
  sleep_disable();
}

void wakeUpISR() {
  RTC_Triggered = true;
}

void initRTC() {
  writeRTC(0x00, 0x00); 
  writeRTC(0x01, 0x01); // TIE = 1
  writeRTC(0x0E, 0x82); // 1Hz source
  writeRTC(0x0F, 0x3C); // 60 Seconds
}

void clearRTCInterrupt() {
  writeRTC(0x01, 0x01); 
}

void updateDisplay(DateTime now, float batt, bool darkMode) {
  digitalWrite(8, LOW); delay(10); digitalWrite(8, HIGH); delay(10);
  display.init(115200, false); 
  display.setPartialWindow(0, 0, display.width(), display.height());
  
  uint16_t bgColor = darkMode ? GxEPD_BLACK : GxEPD_WHITE;
  uint16_t fgColor = darkMode ? GxEPD_WHITE : GxEPD_BLACK;

  display.firstPage();
  do {
    display.fillScreen(bgColor); 
    display.setTextColor(fgColor); 
    
    // --- 1. DATE LINE (FRI MAR 27) ---
    display.setTextSize(2);
    display.setCursor(15, 30);
    
    char dayBuf[4];
    char monthBuf[4];
    // Decays 2D array to pointer correctly for strcpy_P
    strcpy_P(dayBuf, daysOfTheWeek[now.dayOfTheWeek()]);
    strcpy_P(monthBuf, monthNames[now.month()-1]);
    
    display.print(dayBuf);
    display.print(F(" "));
    display.print(monthBuf);
    display.print(F(" "));
    display.print(now.day());
    display.print(F(" "));
    display.print(now.year());

    // --- 2. TIME LINE (14:35) ---
    display.setCursor(15, 100); 
    display.setTextSize(5);   
    if(now.hour() < 10) display.print('0');
    display.print(now.hour());
    display.print(':');
    if(now.minute() < 10) display.print('0');
    display.print(now.minute());
    
    // --- 3. INFO LINE (3.82V) ---
    display.setTextSize(1);
    display.setCursor(80, 165);
    display.print(batt);
    display.print(F("V"));
    
  } while (display.nextPage());
  display.powerOff();
}

void writeRTC(byte reg, byte data) {
  Wire.beginTransmission(0x51);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}