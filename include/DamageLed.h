#pragma once
#include <stdint.h>
#ifndef DAMAGE_LED_RED_PIN
#define DAMAGE_LED_RED_PIN -1
#endif
#ifndef DAMAGE_LED_GREEN_PIN
#define DAMAGE_LED_GREEN_PIN -1
#endif
#ifndef DAMAGE_LED_BLUE_PIN
#define DAMAGE_LED_BLUE_PIN -1
#endif

namespace DamageLed {
constexpr uint32_t flashDurationMs = 180;
bool active = false;
uint32_t startedAt = 0;
void setRed(bool on) {
#ifndef DAMAGE_LED_HOST_TEST
#if DAMAGE_LED_RED_PIN >= 0
  // ELEGOO RGB LED is common anode: LOW lights a channel.
  digitalWrite(DAMAGE_LED_RED_PIN,on ? LOW : HIGH);
#endif
#endif
}
void begin() {
#ifndef DAMAGE_LED_HOST_TEST
  const int pins[]={DAMAGE_LED_RED_PIN,DAMAGE_LED_GREEN_PIN,DAMAGE_LED_BLUE_PIN};
  for (int pin : pins) if (pin>=0) {
    pinMode(pin,OUTPUT);
    digitalWrite(pin,HIGH);
  }
#endif
  active=false;
}
void flash(uint32_t now) {
  startedAt=now;
  active=true;
  setRed(true);
}
void update(uint32_t now) {
  if (active && now-startedAt>=flashDurationMs) {
    active=false;
    setRed(false);
  }
}
}
