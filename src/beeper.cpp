#include "beeper.h"

#include <M5Unified.h>

#include "assets/boom_pcm.h"
#include "config.h"

namespace beeper {
namespace {

constexpr int kBoomChannel = 0;

struct Tone {
  uint16_t freqHz;
  uint16_t onMs;
  uint16_t offMs;
};

constexpr Tone kError[] = {
    {800, 100, 100}, {800, 100, 100}, {800, 100, 100},
};

const Tone* pattern = nullptr;
size_t patternLen = 0;
size_t patternIdx = 0;
uint32_t nextToneMs = 0;

void play(const Tone* p, size_t len) {
  pattern = p;
  patternLen = len;
  patternIdx = 0;
  nextToneMs = 0;
}

}  // namespace

void begin() {
  M5.Speaker.begin();
  M5.Speaker.setVolume(cfg::BEEP_VOLUME);
}

void playFired() {
  pattern = nullptr;  // cancel any beep pattern in progress
  M5.Speaker.stop();
  M5.Speaker.playRaw(kBoomPcm, kBoomPcm_kSampleCount, kBoomPcm_kSampleRate, false, 1, kBoomChannel, true);
}

void playError() { play(kError, sizeof(kError) / sizeof(kError[0])); }

void click() {
  // Never click over the explosion sound or a beep pattern.
  if (pattern == nullptr && !M5.Speaker.isPlaying(kBoomChannel)) M5.Speaker.tone(2000, 20);
}

void update(uint32_t nowMs) {
  if (pattern == nullptr || nowMs < nextToneMs) return;
  if (patternIdx >= patternLen) {
    pattern = nullptr;
    return;
  }
  const Tone& t = pattern[patternIdx++];
  M5.Speaker.tone(t.freqHz, t.onMs);
  nextToneMs = nowMs + t.onMs + t.offMs;
}

}  // namespace beeper
