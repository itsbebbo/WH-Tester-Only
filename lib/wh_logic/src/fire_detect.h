// External-mode FIRED qualification and INA219 health tracking (SPEC §7.4).
// Pure logic, fed one sample per I/O tick.
#pragma once

#include <cstdint>

struct FireSampleResult {
  bool firedNow;       // FIRED latched on this sample
  bool pulseEnded;     // the pulse that caused FIRED has dropped below threshold
  bool firedCleared;   // FIRED cleared after the post-pulse hold (last result kept)
  bool faultNow;       // sensor entered fault on this sample
  bool recoveredNow;   // sensor recovered on this sample
};

class FireDetector {
 public:
  // Clears FIRED, the last-result flag, peak and qualification (RESET /
  // screen entry). Sensor health counters are kept.
  void reset();

  // Abandon any qualification in progress (e.g. relays switching).
  void clearQualification();

  // readOk: I2C read succeeded. detectEnabled: 1 ohm load connected and
  // blanking elapsed.
  FireSampleResult sample(bool readOk, float amps, uint64_t nowUs, bool detectEnabled);

  // FIRED is shown while the current stays >= threshold and for
  // cfg::EXT_FIRED_HOLD_MS after it drops, then clears to a last result.
  bool fired() const { return fired_; }
  bool hasLastFired() const { return lastFired_; }
  uint32_t holdRemainingMs(uint64_t nowUs) const;  // 0 unless counting down
  float firedCurrentA() const { return firedCurrentA_; }
  float peakA() const { return peakA_; }                 // since reset
  float lastPulsePeakA() const { return lastPulsePeakA_; }
  uint32_t lastPulseMs() const { return lastPulseMs_; }  // time above threshold
  bool sensorFault() const { return fault_; }
  uint8_t failCount() const { return failCount_; }

 private:
  bool fired_ = false;
  bool holding_ = false;
  uint64_t holdStartUs_ = 0;
  bool lastFired_ = false;
  float firedCurrentA_ = 0;
  float peakA_ = 0;

  bool aboveActive_ = false;
  bool pulseCausedFire_ = false;
  uint64_t aboveStartUs_ = 0;
  uint16_t samplesAbove_ = 0;
  float pulsePeakA_ = 0;
  float lastPulsePeakA_ = 0;
  uint32_t lastPulseMs_ = 0;

  bool fault_ = false;
  uint8_t failCount_ = 0;
  uint8_t goodCount_ = 0;
};
