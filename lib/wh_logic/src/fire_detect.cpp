#include "fire_detect.h"

#include "config.h"

void FireDetector::reset() {
  fired_ = false;
  holding_ = false;
  lastFired_ = false;
  firedCurrentA_ = 0;
  peakA_ = 0;
  lastPulsePeakA_ = 0;
  lastPulseMs_ = 0;
  clearQualification();
}

void FireDetector::clearQualification() {
  aboveActive_ = false;
  pulseCausedFire_ = false;
  samplesAbove_ = 0;
  pulsePeakA_ = 0;
}

FireSampleResult FireDetector::sample(bool readOk, float amps, uint64_t nowUs, bool detectEnabled) {
  FireSampleResult r{false, false, false, false, false};

  // ---- Sensor health ----
  if (!readOk) {
    goodCount_ = 0;
    if (failCount_ < 255) failCount_++;
    if (!fault_ && failCount_ >= cfg::SENSOR_FAIL_COUNT) {
      fault_ = true;
      r.faultNow = true;
    }
  } else {
    failCount_ = 0;
    if (fault_ && ++goodCount_ >= cfg::SENSOR_RECOVER_COUNT) {
      fault_ = false;
      goodCount_ = 0;
      r.recoveredNow = true;
    }
  }

  const float mag = amps < 0 ? -amps : amps;
  if (readOk && detectEnabled && mag > peakA_) peakA_ = mag;

  // ---- Qualification: >= threshold continuously for FIRE_QUALIFY_MS ----
  // A failed read or a single sample below threshold restarts qualification.
  const bool above = readOk && detectEnabled && amps >= cfg::FIRE_CURRENT_A;
  if (above) {
    if (!aboveActive_) {
      aboveActive_ = true;
      aboveStartUs_ = nowUs;
      samplesAbove_ = 0;
      pulsePeakA_ = 0;
    }
    if (samplesAbove_ < 0xFFFF) samplesAbove_++;
    if (amps > pulsePeakA_) pulsePeakA_ = amps;
    holding_ = false;  // current is back: the hold restarts when it drops again

    if (!fired_ && samplesAbove_ >= cfg::FIRE_MIN_SAMPLES &&
        nowUs - aboveStartUs_ >= uint64_t(cfg::FIRE_QUALIFY_MS) * 1000u) {
      fired_ = true;
      firedCurrentA_ = amps;
      pulseCausedFire_ = true;
      r.firedNow = true;
    }
  } else if (aboveActive_) {
    aboveActive_ = false;
    lastPulseMs_ = uint32_t((nowUs - aboveStartUs_) / 1000u);
    lastPulsePeakA_ = pulsePeakA_;
    if (pulseCausedFire_) {
      pulseCausedFire_ = false;
      r.pulseEnded = true;
    }
    if (fired_) {
      holding_ = true;
      holdStartUs_ = nowUs;
    }
  }

  // ---- FIRED hold: clear EXT_FIRED_HOLD_MS after the current is lost ----
  if (fired_ && holding_ && nowUs - holdStartUs_ >= uint64_t(cfg::EXT_FIRED_HOLD_MS) * 1000u) {
    fired_ = false;
    holding_ = false;
    lastFired_ = true;
    r.firedCleared = true;
  }
  return r;
}

uint32_t FireDetector::holdRemainingMs(uint64_t nowUs) const {
  if (!fired_ || !holding_) return 0;
  const uint64_t elapsedMs = (nowUs - holdStartUs_) / 1000u;
  return elapsedMs >= cfg::EXT_FIRED_HOLD_MS ? 0 : uint32_t(cfg::EXT_FIRED_HOLD_MS - elapsedMs);
}
