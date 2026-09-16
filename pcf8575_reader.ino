#include <Wire.h>

#define PCF8575_ADDR 0x21

void setup() {
    Serial.begin(115200);

    // SDA, SCL
    Wire.begin(19, 23);  // SDA=GPIO19, SCL=GPIO23

    Serial.println("Reading PCF8575...");

    // Set all 16 pins as inputs
    Wire.beginTransmission(PCF8575_ADDR);
    Wire.write(0);  // P0-P7
    Wire.write(0);  // P8-P15
    // Wire.write(0xFF);  // P8-P15
    Wire.endTransmission();
 
}

uint16_t readPCF8575() {
    Wire.requestFrom(PCF8575_ADDR, 2);
    Serial.println(Wire.available());

    if (Wire.available() < 2) {
        Serial.println("Error: No data received from PCF8575.");
        return 0xFFFF;  // error value
    }

    uint8_t lowByte = Wire.read();   // P0-P7
    uint8_t highByte = Wire.read();  // P8-P15

    return ((uint16_t)highByte << 8) | lowByte;
}

void loop() {
    uint16_t pins = readPCF8575();

    Serial.printf("Raw: 0x%04X\n", pins);

    for (int i = 0; i < 16; i++) {
        Serial.printf("P%d = %d\n", i, (pins >> i) & 0x01);
    }

    Serial.println("----------------");
    delay(1000);
}