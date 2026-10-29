#pragma once
#include <stdint.h>
#ifndef GAME_AUDIO_HOST_TEST
#include <Arduino.h>
#include <driver/dac.h>
#include <soc/rtc.h>
#endif

// Set to -1 for boards without an amplifier enable pin.
#ifndef AUDIO_ENABLE_PIN
#define AUDIO_ENABLE_PIN 4 // LCDWIKI ESP32-32E: LOW enables the amplifier.
#endif

// CYD speaker amplifier input: DAC2 / GPIO26. Never enable DAC1 (GPIO25),
// which this project uses as the touchscreen clock. Hardware generates the
// waveform; update() only schedules notes, without delays or sample buffers.
namespace GameAudio {
enum Effect { Click, Start, Shot, Hit, Pop, Feed, Happy, Sleep, Crash,
              Sword, Bow, Hurt, Warning, Spit, Win, Lose, Test };
struct Phrase { uint16_t notes[4]; uint16_t noteMs; uint8_t count, priority; };
const Phrase phrases[] = {
  {{900,0,0,0},45,1,1}, {{392,523,784,0},100,3,2},
  {{1400,800,350,0},35,3,1}, {{330,190,0,0},55,2,2},
  {{550,260,140,0},60,3,2}, {{440,660,550,0},85,3,2},
  {{523,659,784,1047},95,4,2}, {{523,392,262,0},180,3,2},
  {{160,280,140,180},65,4,3}, {{850,500,200,0},35,3,1},
  {{1100,650,400,0},45,3,1}, {{240,0,190,0},85,3,3},
  {{740,0,740,0},110,4,4}, {{380,550,250,0},60,3,1},
  {{523,659,784,1047},160,4,5}, {{392,311,247,165},180,4,5},
  {{440,660,880,0},500,3,6}
};
bool enabled=true, ready=false, playing=false;
bool toneActive=false;
uint8_t volume=4; // 0=mute; 1..4 are the DAC amplitude steps.
uint8_t outputVolume=255;
int volumePercent() { const int levels[]={0,12,25,50,100}; return levels[volume]; }
Effect current=Click;
uint32_t started=0;
uint16_t engineHz=0, outputHz=0;

void enableAmplifier(bool on) {
#ifndef GAME_AUDIO_HOST_TEST
#if AUDIO_ENABLE_PIN >= 0
  digitalWrite(AUDIO_ENABLE_PIN,on ? LOW : HIGH);
#endif
#endif
}

// The DAC driver balances clock references on enable/disable.
void disableTone() {
#ifndef GAME_AUDIO_HOST_TEST
  if (toneActive) dac_cw_generator_disable();
#endif
  toneActive=false;
}

void output(uint16_t hz) {
  if (!ready || (hz==outputHz && outputVolume==volume)) return;
  if (!volume) hz=0;
#ifndef GAME_AUDIO_HOST_TEST
  if (!hz) {
    disableTone();
    dac_output_voltage(DAC_CHANNEL_2,128);
  } else {
    dac_cw_config_t config={};
    config.en_ch=DAC_CHANNEL_2;
    config.scale=(dac_cw_scale_t)(4-volume); // 1/8, 1/4, 1/2, full amplitude.
    config.phase=DAC_CW_PHASE_0;
    config.freq=hz;
    // IDF 4.4 maps offset==0 to register value -128. Use a small explicit
    // nonzero offset to avoid that special case when generating the waveform.
    config.offset=1;
    esp_err_t error=dac_cw_generator_config(&config);
    if (error==ESP_OK && !toneActive) {
      error=dac_cw_generator_enable();
      toneActive=error==ESP_OK;
    }
    if (error!=ESP_OK) {
      Serial.printf("Audio tone failed: %s\n",esp_err_to_name(error));
      disableTone();
      dac_output_voltage(DAC_CHANNEL_2,128);
      enableAmplifier(false);
      ready=false; outputHz=0;
      return;
    }
  }
#endif
  outputHz=hz;
  outputVolume=volume;
}
void begin() {
#ifndef GAME_AUDIO_HOST_TEST
#if AUDIO_ENABLE_PIN >= 0
  pinMode(AUDIO_ENABLE_PIN,OUTPUT);
  digitalWrite(AUDIO_ENABLE_PIN,HIGH);
#endif
  // The legacy CW driver configures the DAC but does not enable its 8 MHz
  // source clock. Preserve the divided clock state used by other peripherals.
  rtc_clk_8m_enable(true,rtc_clk_8md256_enabled());
  esp_err_t error=dac_output_enable(DAC_CHANNEL_2);
  ready=error==ESP_OK;
  if (ready) {
    disableTone();
    dac_output_voltage(DAC_CHANNEL_2,128);
  }
  Serial.printf("Audio DAC2/GPIO26: %s, tone clock: %s\n",
                esp_err_to_name(error),rtc_clk_8m_enabled() ? "on" : "off");
#else
  ready=true;
#endif
  enableAmplifier(ready && enabled && volume>0);
}
void stop() { playing=false; engineHz=0; output(0); }
// Deliberately independent of the CW generator and note scheduler. This
// short, blocking diagnostic runs only from TEST SOUND, never during a game.
bool testDirectDac() {
  stop();
  if (!enabled) return false;
#ifndef GAME_AUDIO_HOST_TEST
  esp_err_t error=dac_output_enable(DAC_CHANNEL_2);
  if (error!=ESP_OK) {
    Serial.printf("Direct DAC test failed: %s\n",esp_err_to_name(error));
    return false;
  }
  enableAmplifier(enabled);
  disableTone();
  const uint16_t frequencies[]={440,660,880};
  for (uint16_t hz : frequencies) {
    const uint32_t halfPeriod=500000UL/hz;
    for (uint16_t cycle=0;cycle<hz/2;++cycle) {
      error=dac_output_voltage(DAC_CHANNEL_2,255);
      if (error!=ESP_OK) break;
      delayMicroseconds(halfPeriod);
      error=dac_output_voltage(DAC_CHANNEL_2,0);
      if (error!=ESP_OK) break;
      delayMicroseconds(halfPeriod);
      if (cycle%32==0) yield();
    }
    dac_output_voltage(DAC_CHANNEL_2,128);
    if (error!=ESP_OK) break;
    delay(100);
  }
  Serial.printf("Direct DAC2/GPIO26 test: %s (440/660/880 Hz)\n",esp_err_to_name(error));
  ready=error==ESP_OK;
#endif
  return ready;
}
void setEnabled(bool value) {
  enabled=value;
  if (!enabled) stop();
  enableAmplifier(enabled && ready && volume>0);
}
void setVolume(int value) {
  volume=value<0 ? 0 : value>4 ? 4 : value;
  if (!volume) stop();
  else output(outputHz);
  enableAmplifier(enabled && ready && volume>0);
}
void update(uint32_t now) {
  if (!enabled || !ready || !volume) { output(0); return; }
  if (playing) {
    const Phrase &p=phrases[current];
    uint32_t note=(uint32_t)(now-started)/p.noteMs;
    if (note<p.count) { output(p.notes[note]); return; }
    playing=false;
  }
  output(engineHz);
}
void play(Effect effect) {
  if (!enabled || !ready || !volume) return;
  uint32_t now=millis();
  update(now);
  if (playing && phrases[effect].priority<phrases[current].priority) return;
  current=effect; started=now; playing=true;
  output(phrases[effect].notes[0]);
}
void engine(float speed) {
  engineHz=enabled && speed>0.5f ? (uint16_t)(140+speed*9) : 0;
}
} // namespace GameAudio
