// Internal Connector mode state machine (SPEC §6). Pure logic: the caller
// supplies debounced inputs and a millisecond clock, so it can be unit tested
// on the host.
#pragma once

#include <cstdint>

#include "tester_types.h"

struct IntInputs {
  bool power;
  bool arm;
  bool fire;
};

struct IntTransition {
  bool changed;
  IntState from;
  IntState to;
  IntError error;
};

class InternalSm {
 public:
  // Entry evaluation (SPEC §6.4) when the Internal screen is entered.
  IntTransition enter(const IntInputs& in, uint32_t nowMs);

  // RESET button: clears FIRED/ERROR and the last-result banner, then
  // re-runs the entry evaluation.
  IntTransition reset(const IntInputs& in, uint32_t nowMs);

  // One evaluation tick (SPEC §6.5). At most one transition per call.
  IntTransition step(const IntInputs& in, uint32_t nowMs);

  // GREEN / RED output levels for the current state (SPEC §6.7).
  void outputs(uint32_t nowMs, bool& green, bool& red) const;

  IntState state() const { return state_; }
  IntError error() const { return error_; }
  uint32_t stateEnteredMs() const { return stateEnteredMs_; }
  uint32_t armedMs() const { return armedMs_; }
  uint32_t firedMs() const { return firedMs_; }
  // Measured ARM->FIRE interval for E_FIRE_TOO_SOON, or for a valid FIRE.
  uint32_t armToFireMs() const { return armToFireMs_; }

  // Seconds left in SETTLING, rounded up (5..1). 0 in other states.
  uint32_t settleRemainingS(uint32_t nowMs) const;

  // Last FIRED/ERROR result. It persists after power loss until RESET or
  // the screen is left, so the operator cannot miss it.
  bool hasLastResult() const { return hasLast_; }
  IntState lastResult() const { return lastState_; }
  IntError lastError() const { return lastError_; }
  uint32_t lastResultMs() const { return lastMs_; }

 private:
  void setState(IntState s, IntError e, uint32_t nowMs, IntTransition& t);
  bool inPowerLossGrace(uint32_t nowMs) const;

  IntState state_ = IntState::NoPower;
  IntError error_ = IntError::None;
  uint32_t stateEnteredMs_ = 0;
  uint32_t armedMs_ = 0;
  uint32_t firedMs_ = 0;
  uint32_t armToFireMs_ = 0;

  bool prevPower_ = false;
  bool powerFell_ = false;
  uint32_t powerFellMs_ = 0;

  bool hasLast_ = false;
  IntState lastState_ = IntState::NoPower;
  IntError lastError_ = IntError::None;
  uint32_t lastMs_ = 0;
};
