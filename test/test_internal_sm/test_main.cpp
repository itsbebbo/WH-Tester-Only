// Unit tests for the Internal-mode state machine (SPEC §6, §13.2).
// Run on a PC:      pio test -e native
// Run on the CoreS3: pio test -e m5stack-cores3
#include <unity.h>

#include "config.h"
#include "internal_sm.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {

InternalSm sm;
uint32_t now;
IntInputs in;

void setIn(bool power, bool arm, bool fire) { in = IntInputs{power, arm, fire}; }

void tick() {
  now++;
  sm.step(in, now);
}

void runFor(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) tick();
}

#define ASSERT_STATE(expected) TEST_ASSERT_EQUAL_INT(int(IntState::expected), int(sm.state()))
#define ASSERT_ERROR(expected) TEST_ASSERT_EQUAL_INT(int(IntError::expected), int(sm.error()))

void assertOutputs(uint32_t t, bool green, bool red) {
  bool g, r;
  sm.outputs(t, g, r);
  TEST_ASSERT_EQUAL_MESSAGE(green, g, "GREEN");
  TEST_ASSERT_EQUAL_MESSAGE(red, r, "RED");
}

void toReady() {
  setIn(true, false, false);
  sm.enter(in, now);
  ASSERT_STATE(Settling);
  runFor(cfg::SETTLING_TIME_MS);
  ASSERT_STATE(Ready);
}

void toArmed() {
  toReady();
  setIn(true, true, false);
  tick();
  ASSERT_STATE(Armed);
}

}  // namespace

void setUp() {
  sm = InternalSm();
  now = 10000;
  setIn(false, false, false);
}

void tearDown() {}

// T-INT-01
void test_entry_all_low_is_no_power() {
  sm.enter(in, now);
  ASSERT_STATE(NoPower);
  assertOutputs(now, false, false);
}

// T-INT-02
void test_power_on_settles_with_green_settling_toggle() {
  sm.enter(in, now);
  setIn(true, false, false);
  tick();
  ASSERT_STATE(Settling);
  const uint32_t t0 = now;
  const uint32_t p = cfg::BLINK_SETTLING_MS;
  TEST_ASSERT_EQUAL_UINT32(5, sm.settleRemainingS(t0));
  assertOutputs(t0, true, false);
  assertOutputs(t0 + p - 1, true, false);
  assertOutputs(t0 + p, false, false);
  assertOutputs(t0 + 2 * p, true, false);
  TEST_ASSERT_EQUAL_UINT32(1, sm.settleRemainingS(t0 + 4001));
}

// T-INT-03
void test_settling_to_ready_at_5000ms_with_green_ready_toggle() {
  setIn(true, false, false);
  sm.enter(in, now);
  runFor(cfg::SETTLING_TIME_MS - 1);
  ASSERT_STATE(Settling);
  tick();
  ASSERT_STATE(Ready);
  const uint32_t t0 = now;
  const uint32_t p = cfg::BLINK_READY_MS;
  assertOutputs(t0, true, false);
  assertOutputs(t0 + p - 1, true, false);
  assertOutputs(t0 + p, false, false);
  assertOutputs(t0 + 2 * p, true, false);
}

// T-INT-04
void test_power_loss_in_settling() {
  setIn(true, false, false);
  sm.enter(in, now);
  runFor(100);
  setIn(false, false, false);
  tick();
  ASSERT_STATE(NoPower);
  assertOutputs(now, false, false);
}

// T-INT-05
void test_power_loss_in_ready() {
  toReady();
  setIn(false, false, false);
  tick();
  ASSERT_STATE(NoPower);
}

// T-INT-06
void test_arm_in_ready_red_steady() {
  toArmed();
  assertOutputs(now, false, true);
  assertOutputs(now + 5000, false, true);
}

// T-INT-07
void test_arm_removed_returns_to_ready() {
  toArmed();
  setIn(true, false, false);
  tick();
  ASSERT_STATE(Ready);
}

// T-INT-08
void test_fire_after_1100ms_is_fired() {
  toArmed();
  runFor(1099);
  setIn(true, true, true);
  tick();
  ASSERT_STATE(Fired);
  TEST_ASSERT_EQUAL_UINT32(1100, sm.armToFireMs());
  assertOutputs(now, false, false);
}

// T-INT-09
void test_fire_after_900ms_is_error_with_fast_blink() {
  toArmed();
  runFor(899);
  setIn(true, true, true);
  tick();
  ASSERT_STATE(Error);
  ASSERT_ERROR(FireTooSoon);
  TEST_ASSERT_EQUAL_UINT32(900, sm.armToFireMs());
  assertOutputs(now, true, true);
  assertOutputs(now + 99, true, true);
  assertOutputs(now + 100, false, false);
  assertOutputs(now + 200, true, true);
}

// T-INT-10
void test_arm_to_fire_boundary() {
  toArmed();
  runFor(998);
  setIn(true, true, true);
  tick();  // 999 ms
  ASSERT_ERROR(FireTooSoon);

  setUp();
  toArmed();
  runFor(999);
  setIn(true, true, true);
  tick();  // 1000 ms
  ASSERT_STATE(Fired);
}

// T-INT-11
void test_fire_in_ready_is_fire_before_arm() {
  toReady();
  setIn(true, false, true);
  tick();
  ASSERT_ERROR(FireBeforeArm);
}

// T-INT-12
void test_arm_and_fire_same_tick_is_fire_before_arm() {
  toReady();
  setIn(true, true, true);
  tick();
  ASSERT_ERROR(FireBeforeArm);
}

// T-INT-13
void test_arm_without_power() {
  sm.enter(in, now);
  setIn(false, true, false);
  tick();
  ASSERT_ERROR(ArmNoPower);
}

// T-INT-14
void test_fire_without_power() {
  sm.enter(in, now);
  setIn(false, false, true);
  tick();
  ASSERT_ERROR(FireNoPower);
}

// T-INT-15
void test_arm_during_settling() {
  setIn(true, false, false);
  sm.enter(in, now);
  runFor(10);
  setIn(true, true, false);
  tick();
  ASSERT_ERROR(ArmSettling);
}

// T-INT-16
void test_entry_with_power_and_arm_is_signal_on_entry() {
  setIn(true, true, false);
  sm.enter(in, now);
  ASSERT_ERROR(SignalOnEntry);
}

void test_entry_without_power_but_arm_is_arm_no_power() {
  setIn(false, true, false);
  sm.enter(in, now);
  ASSERT_ERROR(ArmNoPower);
}

// T-INT-17 + last-result banner
void test_fired_power_loss_keeps_last_result() {
  toArmed();
  runFor(1500);
  setIn(true, true, true);
  tick();
  ASSERT_STATE(Fired);
  setIn(false, false, false);
  tick();
  ASSERT_STATE(NoPower);
  TEST_ASSERT_TRUE(sm.hasLastResult());
  TEST_ASSERT_EQUAL_INT(int(IntState::Fired), int(sm.lastResult()));

  // Survives a further power cycle until RESET
  setIn(true, false, false);
  runFor(cfg::SETTLING_TIME_MS + 10);
  ASSERT_STATE(Ready);
  TEST_ASSERT_TRUE(sm.hasLastResult());
}

// T-INT-18
void test_reset_from_fired_restarts_settling_and_clears_last() {
  toArmed();
  runFor(1500);
  setIn(true, true, true);
  tick();
  setIn(true, false, false);
  runFor(10);
  ASSERT_STATE(Fired);
  sm.reset(in, now);
  ASSERT_STATE(Settling);
  TEST_ASSERT_FALSE(sm.hasLastResult());
  TEST_ASSERT_EQUAL_UINT32(5, sm.settleRemainingS(now));
}

// T-INT-19
void test_error_cleared_by_power_falling_edge() {
  toReady();
  setIn(true, false, true);
  tick();
  ASSERT_STATE(Error);
  setIn(false, false, false);
  tick();
  ASSERT_STATE(NoPower);
  TEST_ASSERT_TRUE(sm.hasLastResult());
  TEST_ASSERT_EQUAL_INT(int(IntError::FireBeforeArm), int(sm.lastError()));
}

// T-INT-20
void test_error_without_power_does_not_self_clear() {
  sm.enter(in, now);
  setIn(false, true, false);
  tick();
  ASSERT_ERROR(ArmNoPower);
  setIn(false, false, false);
  runFor(10000);
  ASSERT_STATE(Error);
}

void test_error_not_cleared_by_power_rising() {
  sm.enter(in, now);
  setIn(false, true, false);
  tick();
  setIn(true, false, false);
  runFor(100);
  ASSERT_STATE(Error);
  setIn(false, false, false);
  tick();
  ASSERT_STATE(NoPower);
}

// T-INT-21
void test_power_loss_grace_allows_arm_to_drop_late() {
  toArmed();
  setIn(false, true, false);
  tick();
  ASSERT_STATE(NoPower);
  runFor(20);
  setIn(false, false, false);
  runFor(100);
  ASSERT_STATE(NoPower);
}

// T-INT-22
void test_arm_held_past_grace_is_error() {
  toArmed();
  setIn(false, true, false);
  tick();
  const uint32_t fell = now;
  while (now - fell < cfg::POWER_LOSS_GRACE_MS - 1) {
    tick();
    ASSERT_STATE(NoPower);
  }
  tick();
  ASSERT_ERROR(ArmNoPower);
}

void test_fired_ignores_arm_and_fire_activity() {
  toArmed();
  runFor(1200);
  setIn(true, true, true);
  tick();
  setIn(true, false, false);
  runFor(50);
  setIn(true, true, false);
  runFor(50);
  setIn(true, false, true);
  runFor(50);
  ASSERT_STATE(Fired);
}

void test_error_keeps_first_error_code() {
  toReady();
  setIn(true, false, true);
  tick();
  ASSERT_ERROR(FireBeforeArm);
  setIn(true, true, true);
  runFor(3000);
  ASSERT_ERROR(FireBeforeArm);
}

int runAllTests() {
  UNITY_BEGIN();
  RUN_TEST(test_entry_all_low_is_no_power);
  RUN_TEST(test_power_on_settles_with_green_settling_toggle);
  RUN_TEST(test_settling_to_ready_at_5000ms_with_green_ready_toggle);
  RUN_TEST(test_power_loss_in_settling);
  RUN_TEST(test_power_loss_in_ready);
  RUN_TEST(test_arm_in_ready_red_steady);
  RUN_TEST(test_arm_removed_returns_to_ready);
  RUN_TEST(test_fire_after_1100ms_is_fired);
  RUN_TEST(test_fire_after_900ms_is_error_with_fast_blink);
  RUN_TEST(test_arm_to_fire_boundary);
  RUN_TEST(test_fire_in_ready_is_fire_before_arm);
  RUN_TEST(test_arm_and_fire_same_tick_is_fire_before_arm);
  RUN_TEST(test_arm_without_power);
  RUN_TEST(test_fire_without_power);
  RUN_TEST(test_arm_during_settling);
  RUN_TEST(test_entry_with_power_and_arm_is_signal_on_entry);
  RUN_TEST(test_entry_without_power_but_arm_is_arm_no_power);
  RUN_TEST(test_fired_power_loss_keeps_last_result);
  RUN_TEST(test_reset_from_fired_restarts_settling_and_clears_last);
  RUN_TEST(test_error_cleared_by_power_falling_edge);
  RUN_TEST(test_error_without_power_does_not_self_clear);
  RUN_TEST(test_error_not_cleared_by_power_rising);
  RUN_TEST(test_power_loss_grace_allows_arm_to_drop_late);
  RUN_TEST(test_arm_held_past_grace_is_error);
  RUN_TEST(test_fired_ignores_arm_and_fire_activity);
  RUN_TEST(test_error_keeps_first_error_code);
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
