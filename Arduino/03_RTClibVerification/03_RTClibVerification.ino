#include <Wire.h>
#include "RTClib.h"

RTC_PCF8563 rtc;

void setup() {
  Serial.begin(9600);
  delay(1000); // Wait for console

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // The following line sets the RTC to the date & time this sketch was compiled
  // IMPORTANT: Run this once, then comment it out and re-upload!
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  Serial.println("RTC Time Set!");
}

void loop() {
  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);

  delay(1000);
}