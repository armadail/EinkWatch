#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "RTClib.h"

GxEPD2_BW<GxEPD2_154_D67, 32> display(GxEPD2_154_D67(/*CS=*/ 10, /*DC=*/ 9, /*RST=*/ 8, /*BUSY=*/ 7));

RTC_PCF8563 rtc;

// Function to measure VCC (Battery Voltage) without external pins
float getBatteryVoltage() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // Wait for conversion
  uint8_t low  = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  
  // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  float vcc = 1314371L / result; 
  return vcc / 1000.0; // Return as Volts (e.g. 3.72)
}

void setup() {
  Wire.begin();
  rtc.begin();
  
  display.init(0); // Passing 0 removes serial debug from the library
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);
}

void loop() {
  DateTime now = rtc.now();
  float batt = getBatteryVoltage();

  display.setFullWindow();
  display.firstPage(); 
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // --- Draw Time ---
    display.setCursor(25, 80);
    display.setTextSize(5);
    if(now.hour() < 10) display.print('0');
    display.print(now.hour());
    display.print(':');
    if(now.minute() < 10) display.print('0');
    display.print(now.minute());
    
    // --- Draw Battery ---
    display.setTextSize(1);
    display.setCursor(75, 180);
    display.print(batt);
    display.print("V");
    
  } while (display.nextPage()); 

  // Wait 1 minute for the next update
  delay(10000); 
}