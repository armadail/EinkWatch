void setup() {
  Serial.begin(9600); // If this looks like gibberish, your 8MHz fuse didn't set correctly
}

void loop() {
  Serial.println("Watch Brain is Alive!");
  delay(1000); 
}