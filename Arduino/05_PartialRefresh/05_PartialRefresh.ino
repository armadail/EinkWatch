#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "RTClib.h"

GxEPD2_BW<GxEPD2_154_D67, 32> display(GxEPD2_154_D67(/*CS=*/ 10, /*DC=*/ 9, /*RST=*/ 8, /*BUSY=*/ 7));
RTC_PCF8563 rtc;

int lastSecond = -1; 

float getBatteryVoltage() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); 
  ADCSRA |= _BV(ADSC); 
  while (bit_is_set(ADCSRA, ADSC)); 
  uint8_t low  = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  // Using your calibrated constant
  float vcc = 1400000L / result; 
  return vcc / 1000.0; 
}

void setup() {
  Wire.begin();
  rtc.begin();
  
  display.init(0, true); 
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);
  
  // Initial Full Clear
  display.setFullWindow();
  display.firstPage();
  do {
     display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
}

void loop() {
  DateTime now = rtc.now();

  // Update every single second
  if (now.second() != lastSecond) {
    lastSecond = now.second();
    float batt = getBatteryVoltage();

    // Partial window prevents the black/white flash
    display.setPartialWindow(0, 0, display.width(), display.height());
    
    display.firstPage(); 
    do {
      display.fillScreen(GxEPD_WHITE);
      
      // --- Time (HH:MM:SS) ---
      display.setCursor(5, 80); // Moved left to fit seconds
      display.setTextSize(4);   // Slightly smaller to fit 8 characters
      
      if(now.hour() < 10) display.print('0');
      display.print(now.hour());
      display.print(':');
      if(now.minute() < 10) display.print('0');
      display.print(now.minute());
      display.print(':');
      if(now.second() < 10) display.print('0');
      display.print(now.second());
      
      // --- Battery ---
      display.setTextSize(1);
      display.setCursor(75, 180);
      display.print(batt);
      display.print("V");
      
    } while (display.nextPage());
  }
  // No delay needed, the loop will spin until the next second
}