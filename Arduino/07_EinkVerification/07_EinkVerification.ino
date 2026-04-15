#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "RTClib.h"

// --- DRIVER SELECTION ---
// Try the GDEH0154D67 driver first (standard for many WeAct 1.54" modules)
GxEPD2_BW<GxEPD2_154_D67, 32> display(GxEPD2_154_D67(/*CS=*/ 10, /*DC=*/ 9, /*RST=*/ 8, /*BUSY=*/ 7));

// IF THE ABOVE FAILS, comment it out and uncomment the line below:
// GxEPD2_BW<GxEPD2_154_GDEW0154M09, 32> display(GxEPD2_154_GDEW0154M09(/*CS=*/ 10, /*DC=*/ 9, /*RST=*/ 8, /*BUSY=*/ 7));

RTC_PCF8563 rtc;

void setup() {
  Serial.begin(57600);
  while (!Serial); // Wait for Serial Monitor to open
  Serial.println("--- E-Ink Watch Debug Start ---");

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }

  Serial.println("Initializing Display...");
  // The (57600) here is for debug output, (true) enables partial refresh
  display.init(57600, true); 
  
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);
  Serial.println("Display Setup Complete.");
}

void loop() {
  DateTime now = rtc.now();
  
  // Debug print to Serial so we know the chip is alive
  Serial.print("Current Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.println(now.minute());

  Serial.println("Starting E-Ink Refresh...");
  
  display.setFullWindow();
  display.firstPage(); 
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(20, 80);
    display.setTextSize(4);
    
    if(now.hour() < 10) display.print('0');
    display.print(now.hour());
    display.print(':');
    if(now.minute() < 10) display.print('0');
    display.print(now.minute());
    
  } while (display.nextPage()); 

  Serial.println("Refresh Finished. Waiting 10 seconds...");
  delay(10000); // 10s for testing, change to 60000 (1 min) for the final watch
}