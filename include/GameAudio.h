#pragma once
#include <stdint.h>
#ifndef GAME_AUDIO_HOST_TEST
#include <Arduino.h>
#include <driver/dac.h>
#include <soc/rtc.h>
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
Effect current=Click;
uint32_t started=0;
uint16_t engineHz=0, outputHz=0;

void output(uint16_t hz) {
  if (!ready || hz==outputHz) return;
#ifndef GAME_AUDIO_HOST_TEST
  if (!hz) {
    dac_cw_generator_disable();
    dac_output_voltage(DAC_CHANNEL_2,128);
  } else {
    dac_cw_config_t config={};
    config.en_ch=DAC_CHANNEL_2;
    config.scale=DAC_CW_SCALE_1; // Full amplitude (4x the previous DAC signal).
    config.phase=DAC_CW_PHASE_0;
    config.freq=hz;
    // IDF 4.4 maps offset==0 to register value -128. Use a small explicit
    // nonzero offset to avoid that special case when generating the waveform.
    config.offset=1;
    esp_err_t error=dac_cw_generator_config(&config);
    if (error==ESP_OK) error=dac_cw_generator_enable();
    if (error!=ESP_OK) {
      Serial.printf("Audio tone failed: %s\n",esp_err_to_name(error));
      dac_cw_generator_disable();
      dac_output_voltage(DAC_CHANNEL_2,128);
      ready=false; outputHz=0;
      return;
    }
  }
#endif
  outputHz=hz;
}
void begin() {
#ifndef GAME_AUDIO_HOST_TEST
  // The legacy CW driver configures the DAC but does not enable its 8 MHz
  // source clock. Preserve the divided clock state used by other peripherals.
  rtc_clk_8m_enable(true,rtc_clk_8md256_enabled());
  esp_err_t error=dac_output_enable(DAC_CHANNEL_2);
  ready=error==ESP_OK;
  if (ready) {
    dac_cw_generator_disable();
    dac_output_voltage(DAC_CHANNEL_2,128);
  }
  Serial.printf("Audio DAC2/GPIO26: %s, tone clock: %s\n",
                esp_err_to_name(error),rtc_clk_8m_enabled() ? "on" : "off");
#else
  ready=true;
#endif
}
void stop() { playing=false; engineHz=0; output(0); }
// Deliberately independent of the CW generator and note scheduler. This
// short, blocking diagnostic runs only from TEST SOUND, never during a game.
bool testDirectDac() {
  stop();
#ifndef GAME_AUDIO_HOST_TEST
  esp_err_t error=dac_output_enable(DAC_CHANNEL_2);
  if (error!=ESP_OK) {
    Serial.printf("Direct DAC test failed: %s\n",esp_err_to_name(error));
    return false;
  }
  dac_cw_generator_disable();
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
void setEnabled(bool value) { enabled=value; if (!enabled) stop(); }
void update(uint32_t now) {
  if (!enabled || !ready) { output(0); return; }
  if (playing) {
    const Phrase &p=phrases[current];
    uint32_t note=(uint32_t)(now-started)/p.noteMs;
    if (note<p.count) { output(p.notes[note]); return; }
    playing=false;
  }
  output(engineHz);
}
void play(Effect effect) {
  if (!enabled || !ready) return;
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
