// Unit tests for External-mode FIRED qualification (SPEC §7.4, §13.3).
// Run on a PC:      pio test -e native
// Run on the CoreS3: pio test -e m5stack-cores3
#include <unity.h>

#include "config.h"
#include "fire_detect.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {

FireDetector fd;
uint64_t nowUs;
int firedEvents;
int pulseEndEvents;
int clearedEvents;
int faultEvents;
int recoverEvents;

// Feeds `count` samples 1 ms apart. Returns the sample index (0-based) at
// which FIRED latched, or -1.
int feed(float amps, int count, bool ok = true, bool enabled = true) {
  int firedAt = -1;
  for (int i = 0; i < count; i++) {
    const FireSampleResult r = fd.sample(ok, amps, nowUs, enabled);
    if (r.firedNow) {
      firedEvents++;
      firedAt = i;
    }
    if (r.pulseEnded) pulseEndEvents++;
    if (r.firedCleared) clearedEvents++;
    if (r.faultNow) faultEvents++;
    if (r.recoveredNow) recoverEvents++;
    nowUs += 1000;
  }
  return firedAt;
}

}  // namespace

void setUp() {
  fd = FireDetector();
  nowUs = 1000000;
  firedEvents = pulseEndEvents = clearedEvents = faultEvents = recoverEvents = 0;
}

void tearDown() {}

// T-EXT-05
void test_steady_1A_never_fires() {
  feed(1.0f, 1000);
  TEST_ASSERT_FALSE(fd.fired());
}

// T-EXT-06
void test_3ms_pulse_does_not_fire() {
  feed(1.5f, 3);
  feed(0.0f, 10);
  TEST_ASSERT_FALSE(fd.fired());
}

// T-EXT-07
void test_8ms_pulse_fires_at_5ms() {
  const int firedAt = feed(1.5f, 8);
  TEST_ASSERT_TRUE(fd.fired());
  TEST_ASSERT_EQUAL_INT(int(cfg::FIRE_QUALIFY_MS), firedAt);  // 5 ms after the first sample
  TEST_ASSERT_EQUAL_INT(1, firedEvents);
}

void test_exact_threshold_counts_as_above() {
  feed(cfg::FIRE_CURRENT_A, 10);
  TEST_ASSERT_TRUE(fd.fired());
}

void test_just_below_threshold_never_fires() {
  feed(cfg::FIRE_CURRENT_A - 0.001f, 100);
  TEST_ASSERT_FALSE(fd.fired());
}

// T-EXT-08
void test_dropouts_restart_qualification() {
  for (int cycle = 0; cycle < 50; cycle++) {
    feed(1.5f, 2);
    feed(0.5f, 1);
  }
  TEST_ASSERT_FALSE(fd.fired());
}

void test_failed_read_restarts_qualification() {
  feed(1.5f, 4);
  feed(1.5f, 1, false);
  feed(1.5f, 4);
  TEST_ASSERT_FALSE(fd.fired());
  feed(1.5f, 2);
  TEST_ASSERT_TRUE(fd.fired());
}

void test_detection_disabled_never_fires() {
  feed(3.0f, 100, true, false);
  TEST_ASSERT_FALSE(fd.fired());
}

void test_fired_held_while_current_stays_above() {
  feed(1.5f, 20000);
  TEST_ASSERT_TRUE(fd.fired());
  TEST_ASSERT_EQUAL_UINT32(0, fd.holdRemainingMs(nowUs));
}

void test_fired_held_for_hold_time_after_current_lost() {
  feed(1.5f, 10);
  feed(0.0f, 1);  // current lost: hold starts on this sample
  TEST_ASSERT_EQUAL_UINT32(cfg::EXT_FIRED_HOLD_MS - 1, fd.holdRemainingMs(nowUs));
  feed(0.0f, cfg::EXT_FIRED_HOLD_MS - 1);
  TEST_ASSERT_TRUE(fd.fired());
  TEST_ASSERT_FALSE(fd.hasLastFired());
  feed(0.0f, 1);  // exactly EXT_FIRED_HOLD_MS after the drop
  TEST_ASSERT_FALSE(fd.fired());
  TEST_ASSERT_TRUE(fd.hasLastFired());
  TEST_ASSERT_EQUAL_INT(1, clearedEvents);
}

void test_current_returning_during_hold_restarts_hold() {
  feed(1.5f, 10);
  feed(0.0f, 3000);
  feed(1.5f, 2);  // current back: hold cancelled
  TEST_ASSERT_EQUAL_UINT32(0, fd.holdRemainingMs(nowUs));
  feed(0.0f, cfg::EXT_FIRED_HOLD_MS);
  TEST_ASSERT_TRUE(fd.fired());
  feed(0.0f, 1);
  TEST_ASSERT_FALSE(fd.fired());
  TEST_ASSERT_EQUAL_INT(1, firedEvents);  // re-rise during hold is not a new firing
}

void test_can_fire_again_after_clear() {
  feed(1.5f, 10);
  feed(0.0f, cfg::EXT_FIRED_HOLD_MS + 10);
  TEST_ASSERT_FALSE(fd.fired());
  feed(1.5f, 10);
  TEST_ASSERT_TRUE(fd.fired());
  TEST_ASSERT_EQUAL_INT(2, firedEvents);
  TEST_ASSERT_TRUE(fd.hasLastFired());
}

void test_reset_clears_last_fired() {
  feed(1.5f, 10);
  feed(0.0f, cfg::EXT_FIRED_HOLD_MS + 10);
  TEST_ASSERT_TRUE(fd.hasLastFired());
  fd.reset();
  TEST_ASSERT_FALSE(fd.hasLastFired());
}

void test_pulse_end_reports_peak_and_duration() {
  feed(1.4f, 5);
  feed(1.9f, 5);
  feed(1.3f, 10);
  feed(0.2f, 1);
  TEST_ASSERT_EQUAL_INT(1, pulseEndEvents);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.9f, fd.lastPulsePeakA());
  TEST_ASSERT_EQUAL_UINT32(20, fd.lastPulseMs());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.9f, fd.peakA());
}

void test_reset_clears_latch_and_peak() {
  feed(1.5f, 10);
  TEST_ASSERT_TRUE(fd.fired());
  fd.reset();
  TEST_ASSERT_FALSE(fd.fired());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, fd.peakA());
  feed(1.5f, 10);
  TEST_ASSERT_TRUE(fd.fired());
}

void test_clear_qualification_mid_pulse() {
  feed(1.5f, 4);
  fd.clearQualification();
  feed(1.5f, 4);
  TEST_ASSERT_FALSE(fd.fired());
}

// T-EXT-11
void test_sensor_fault_after_3_failures_and_recovery_after_10_good() {
  feed(0.0f, cfg::SENSOR_FAIL_COUNT - 1, false);
  TEST_ASSERT_FALSE(fd.sensorFault());
  feed(0.0f, 1, false);
  TEST_ASSERT_TRUE(fd.sensorFault());
  TEST_ASSERT_EQUAL_INT(1, faultEvents);

  feed(0.0f, cfg::SENSOR_RECOVER_COUNT - 1);
  TEST_ASSERT_TRUE(fd.sensorFault());
  feed(0.0f, 1);
  TEST_ASSERT_FALSE(fd.sensorFault());
  TEST_ASSERT_EQUAL_INT(1, recoverEvents);
}

void test_recovery_count_restarts_on_failure() {
  feed(0.0f, 3, false);
  feed(0.0f, 9);
  feed(0.0f, 1, false);
  feed(0.0f, 9);
  TEST_ASSERT_TRUE(fd.sensorFault());
  feed(0.0f, 1);
  TEST_ASSERT_FALSE(fd.sensorFault());
}

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_steady_1A_never_fires);
  RUN_TEST(test_3ms_pulse_does_not_fire);
  RUN_TEST(test_8ms_pulse_fires_at_5ms);
  RUN_TEST(test_exact_threshold_counts_as_above);
  RUN_TEST(test_just_below_threshold_never_fires);
  RUN_TEST(test_dropouts_restart_qualification);
  RUN_TEST(test_failed_read_restarts_qualification);
  RUN_TEST(test_detection_disabled_never_fires);
  RUN_TEST(test_fired_held_while_current_stays_above);
  RUN_TEST(test_fired_held_for_hold_time_after_current_lost);
  RUN_TEST(test_current_returning_during_hold_restarts_hold);
  RUN_TEST(test_can_fire_again_after_clear);
  RUN_TEST(test_reset_clears_last_fired);
  RUN_TEST(test_pulse_end_reports_peak_and_duration);
  RUN_TEST(test_reset_clears_latch_and_peak);
  RUN_TEST(test_clear_qualification_mid_pulse);
  RUN_TEST(test_sensor_fault_after_3_failures_and_recovery_after_10_good);
  RUN_TEST(test_recovery_count_restarts_on_failure);
  return UNITY_END();
}

#ifdef ARDUINO
void setup() {
  delay(2000);  // allow the USB serial monitor to attach
  runAllTests();
}
void loop() {}
#else
int main() { return runAllTests(); }
#endif
