#include <Arduino.h>
#include <Wire.h>

#define PCF8574_ADDRESS 0x20

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nPCF8574 Pin Reader");
  Wire.begin(19, 23);  // SDA=GPIO19, SCL=GPIO23
  Wire.setClock(100000);  // Set I2C clock to 100kHz (slower, more reliable)
  delay(200);  // Give I2C bus time to stabilize
  
  Serial.println("Reading PCF8574 pins continuously...");
  Serial.println("Push buttons/move joystick to see values change\n");
}

void loop() {
  static uint8_t lastValue = 0xFF;  // Track previous value
  
  delay(50);  // Fast poll to catch changes
  
  // Request 1 byte from PCF8574
  int bytes_read = Wire.requestFrom(PCF8574_ADDRESS, 1, true);
  
  if (bytes_read > 0 && Wire.available()) {
    uint8_t value = Wire.read();
    
    // Only print if value changed
    if (value != lastValue) {
      Serial.print("Change detected! Value: 0x");
      if (value < 16) Serial.print("0");
      Serial.print(value, HEX);
      Serial.print(" (binary: ");
      
      // Print binary representation
      for (int i = 7; i >= 0; i--) {
        Serial.print((value & (1 << i)) ? "1" : "0");
      }
      Serial.println(")");
      
      // Show which pins changed
      uint8_t changed = value ^ lastValue;
      for (int pin = 0; pin < 16; pin++) {
        if (changed & (1 << pin)) {
          Serial.print("  Pin ");
          Serial.print(pin);
          Serial.print(" changed to: ");
          Serial.println(((value & (1 << pin)) == 0) ? "LOW (pressed)" : "HIGH (released)");
        }
      }
      Serial.println("---");
      
      lastValue = value;
    }
  }
}
