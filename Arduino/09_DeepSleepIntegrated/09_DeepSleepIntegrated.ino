#include <Wire.h>
#include <avr/sleep.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include "RTClib.h"

// 32-row page height to keep SRAM usage around 1KB
GxEPD2_BW<GxEPD2_154_D67, 32> display(GxEPD2_154_D67(10, 9, 8, 7));
RTC_PCF8563 rtc;

volatile bool RTC_Triggered = false;
bool isDarkMode = true; // NEW: Tracks the current visual state

// Reads internal 1.1V reference to calculate VCC
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
  Serial.println(F("\n--- VANCOUVER WATCH: SYSTEM START ---"));

  pinMode(6, OUTPUT); // Status LED
  pinMode(2, INPUT_PULLUP); // RTC Interrupt Pin

  // 1. I2C & RTC SETUP
  Wire.begin();
  Wire.setClock(10000); // 10kHz for max stability
  rtc.begin();
  initRTC(); // Configure sleep timer

  // 2. INITIAL FULL REFRESH (Clears ghosting)
  display.init(115200, true, 2, false); 
  display.setRotation(3);
  display.setFullWindow();
  display.firstPage();
  do {
    // Dark Mode Background
    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setTextSize(2);
    display.setCursor(10, 80);
    display.print(F("WATCH ACTIVE"));
  } while (display.nextPage());
  display.powerOff();

  // 3. ATTACH INTERRUPT
  attachInterrupt(digitalPinToInterrupt(2), wakeUpISR, LOW);
  
  Serial.println(F("Setup Complete. Entering Sleep..."));
  Serial.flush();
}

void loop() {
  if (RTC_Triggered) {
    detachInterrupt(digitalPinToInterrupt(2)); // Stop the madness
    digitalWrite(6, HIGH); 
    
    clearRTCInterrupt(); 
    
    DateTime now = rtc.now();
    float batt = getBatteryVoltage();
    isDarkMode = !isDarkMode; 
    updateDisplay(now, batt, isDarkMode);
    
    RTC_Triggered = false;
    digitalWrite(6, LOW);
    
    // 3. RE-ATTACH right before going back to sleep
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
  // 1. Reset Control 1 (Standard start)
  writeRTC(0x00, 0x00); 
  
  // 2. Control 2 (Register 0x01)
  // We need Bit 0 (TIE) = 1 to allow the timer to trigger the INT pin.
  // 0x01 = 0000 0001
  writeRTC(0x01, 0x01); 

  // 3. Timer Control (Register 0x0E)
  // TE=1, TD=10 (1Hz clock)
  writeRTC(0x0E, 0x82); 
  
  // 4. Set Timer to 60 seconds
  writeRTC(0x0F, 0x3C); 

  Serial.println(F("RTC: Hardware Interrupts Enabled (60s)"));
}

void clearRTCInterrupt() {
  // IMPORTANT: We must keep TIE (Bit 0) enabled while clearing the flag (Bit 3)
  // If we write 0x00, we turn off the interrupt capability!
  // Write 0x01 to keep Timer Interrupt Enabled but clear the Flag.
  writeRTC(0x01, 0x01); 
  Serial.println(F("RTC: Flag Cleared, Interrupts Armed."));
}

// NEW: Add the boolean parameter
void updateDisplay(DateTime now, float batt, bool darkMode) {
  digitalWrite(8, LOW); delay(10); digitalWrite(8, HIGH); delay(10);
  
  display.init(115200, false); 
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  
  // NEW: Ternary operators to swap foreground and background
  uint16_t bgColor = darkMode ? GxEPD_BLACK : GxEPD_WHITE;
  uint16_t fgColor = darkMode ? GxEPD_WHITE : GxEPD_BLACK;

  do {
    display.fillScreen(bgColor); 
    display.setTextColor(fgColor); 
    
    // --- Time (HH:MM) ---
    display.setCursor(15, 80); 
    display.setTextSize(5);   
    
    if(now.hour() < 10) display.print('0');
    display.print(now.hour());
    display.print(':');
    if(now.minute() < 10) display.print('0');
    display.print(now.minute());
    
    // --- Battery ---
    display.setTextSize(2);
    display.setCursor(60, 160);
    display.print(batt);
    display.print(F("V"));
    
  } while (display.nextPage());
  display.powerOff();
}

void writeRTC(byte reg, byte data) {
  Wire.beginTransmission(0x51);
  Wire.write(reg);
  Wire.write(data);
  byte result = Wire.endTransmission();
  
  if (result != 0) {
    Serial.print(F("I2C WRITE ERROR: "));
    Serial.println(result);
    // Force a bus reset if it hangs
    Wire.begin(); 
  }
}