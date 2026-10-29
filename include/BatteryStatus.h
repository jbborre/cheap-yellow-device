#pragma once
#include <stdint.h>
#ifndef BATTERY_ADC_PIN
#define BATTERY_ADC_PIN -1
#endif
#ifndef BATTERY_DIVIDER
#define BATTERY_DIVIDER 2.0f
#endif

namespace BatteryStatus {
int percentage = -1;
uint32_t lastRead = 0;
// Approximate single-cell Li-ion state of charge from voltage under light load.
// Charging, temperature, and load affect this estimate.
int fromMillivolts(uint32_t mv) {
  const uint16_t voltage[] = {3300,3500,3600,3700,3750,3800,3850,3900,4000,4100,4200};
  const uint8_t charge[] = {0,5,10,20,30,40,50,60,75,90,100};
  if (mv <= voltage[0]) return 0;
  for (unsigned i=1;i<sizeof(voltage)/sizeof(voltage[0]);++i)
    if (mv<=voltage[i])
      return charge[i-1]+(mv-voltage[i-1])*(charge[i]-charge[i-1])/(voltage[i]-voltage[i-1]);
  return 100;
}
#ifndef BATTERY_HOST_TEST
bool update(uint32_t now, bool force=false) {
#if BATTERY_ADC_PIN >= 0
  if (!force && now-lastRead<5000) return false;
  lastRead=now;
  uint32_t total=0;
  for (int i=0;i<16;++i) total+=analogReadMilliVolts(BATTERY_ADC_PIN);
  uint32_t mv=(uint32_t)(total/16.0f*BATTERY_DIVIDER);
  int next=(mv<2000 || mv>4500) ? -1 : fromMillivolts(mv);
  bool changed=next!=percentage;
  percentage=next;
  return changed;
#else
  return false;
#endif
}
void begin() {
#if BATTERY_ADC_PIN >= 0
  pinMode(BATTERY_ADC_PIN,INPUT);
  analogSetPinAttenuation(BATTERY_ADC_PIN,ADC_11db);
  update(millis(),true);
  Serial.printf("Battery: GPIO%d, estimate %d%% (-1=unavailable)\n",BATTERY_ADC_PIN,percentage);
#endif
}
#endif
}
