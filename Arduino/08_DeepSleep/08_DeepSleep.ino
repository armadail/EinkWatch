#include <Wire.h>
#include <avr/sleep.h>
#include <GxEPD2_BW.h>

// 32-row page height to keep SRAM usage around 1KB
GxEPD2_BW<GxEPD2_154_D67, 32> display(GxEPD2_154_D67(10, 9, 8, 7));

volatile bool RTC_Triggered = false;

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println(F("\n--- VANCOUVER WATCH: STABLE BUILD ---"));

  pinMode(6, OUTPUT); // Status LED
  pinMode(2, INPUT_PULLUP); // RTC Interrupt Pin

  // 1. I2C & RTC SETUP
  Wire.begin();
  Wire.setClock(10000); // 10kHz for max stability
  initRTC();

  // 2. INITIAL FULL REFRESH (Clears ghosting)
  display.init(115200);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_BLACK);
    display.setCursor(50, 100);
    display.print(F("WATCH ACTIVE"));
  } while (display.nextPage());
  display.powerOff();

  // 3. ATTACH INTERRUPT
  // Use FALLING to avoid the "LOW" level loop bug
  attachInterrupt(digitalPinToInterrupt(2), wakeUpISR, FALLING);
  
  Serial.println(F("Setup Complete. Entering Sleep..."));
  Serial.flush();
}

void loop() {
  if (RTC_Triggered) {
    digitalWrite(6, HIGH); // Signal Wakeup
    Serial.println(F("Wakeup Triggered!"));
    
    // Clear RTC Flag immediately so Pin 2 goes HIGH
    clearRTCInterrupt();
    
    // Refresh Display
    updateDisplay();
    
    RTC_Triggered = false;
    digitalWrite(6, LOW);
    Serial.println(F("Going back to sleep..."));
    Serial.flush();
  }

  // DEEP SLEEP
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu(); 
  
  // MCU STOPS HERE UNTIL PIN 2 FALLS
  
  sleep_disable();
}

void wakeUpISR() {
  RTC_Triggered = true;
}

void initRTC() {
  // Address 0x51 (PCF8563)
  writeRTC(0x00, 0x00); // Control 1
  writeRTC(0x01, 0x00); // Control 2
  writeRTC(0x0E, 0x82); // Timer: 1Hz source, Enabled
  writeRTC(0x0F, 0x0A); // 10 Seconds
}

void clearRTCInterrupt() {
  // Write 0 to Status 2 to clear Timer Flag (Bit 3)
  writeRTC(0x01, 0x00); 
}

void updateDisplay() {
  digitalWrite(8, LOW); delay(10); digitalWrite(8, HIGH); delay(10);
  
  display.init(115200, false); 
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    display.fillScreen(GxEPD_BLACK); // Dark Mode Background
    display.setTextColor(GxEPD_WHITE); // White Text
    display.setCursor(40, 100);
    display.print(F("10:47 AM")); // Static time for now
  } while (display.nextPage());
  display.powerOff();
}

void writeRTC(byte reg, byte data) {
  Wire.beginTransmission(0x51);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}