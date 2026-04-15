#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial); 
  Serial.println("\nI2C Scanner");
}

void loop() {
  byte error, address;
  Serial.println("Scanning...");

  // The PCF8563 default I2C address is 0x51
  address = 0x51; 

  Wire.beginTransmission(address);
  error = Wire.endTransmission();

  if (error == 0) {
    Serial.print("I2C device found at address 0x");
    Serial.println(address, HEX);
  } else {
    Serial.println("No I2C devices found. Check wiring/pull-ups.");
  }

  delay(5000);
}