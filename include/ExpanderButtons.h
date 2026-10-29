#pragma once
#include <Wire.h>

// PCF8574T, buttons between P0-P3 and GND.
// Default 0x20 assumes A0-A2 are LOW; override for other address straps.
#ifndef GAME_BUTTONS_ADDRESS
#define GAME_BUTTONS_ADDRESS 0x20
#endif
namespace ExpanderButtons {
uint8_t held = 0, pressed = 0, candidate = 0;
uint32_t changedAt = 0, lastPoll = 0;
uint8_t address = GAME_BUTTONS_ADDRESS;
bool connected = false;
void begin() {
  Wire.begin(19, 23); // SDA, SCL
  Wire.setTimeOut(10);
  // Try the configured address first, then the PCF8574T address straps.
  for (int attempt = 0; attempt <= 8; ++attempt) {
    uint8_t probe = attempt == 0 ? GAME_BUTTONS_ADDRESS : 0x20 + attempt - 1;
    if (attempt && probe == GAME_BUTTONS_ADDRESS) continue;
    Wire.beginTransmission(probe);
    // Quasi-bidirectional inputs must be written HIGH to release the pins.
    Wire.write(0xFF);
    if (Wire.endTransmission() == 0) {
      address = probe;
      connected = true;
      Serial.printf("Button expander: PCF8574T at 0x%02X, SDA=19 SCL=23\n", address);
      return;
    }
  }
  Serial.println("Button expander: no response at 0x20-0x27 (SDA=19 SCL=23)");
}
void update(uint32_t now) {
  pressed = 0;
  if (!connected || now - lastPoll < 5) return;
  lastPoll = now;
  if (Wire.requestFrom(address, (uint8_t)1) != 1) {
    while (Wire.available()) Wire.read();
    held = candidate = 0; // A disconnected expander must not leave controls held.
    changedAt = now;
    return;
  }
  uint8_t sample = (~Wire.read()) & 0x0F;
  if (sample != candidate) { candidate = sample; changedAt = now; }
  if (now - changedAt >= 20) {
    pressed = candidate & ~held;
    if (held != candidate)
      Serial.printf("Buttons P0-P3 pressed: 0x%X\n", candidate);
    held = candidate;
  }
}
} // namespace ExpanderButtons
