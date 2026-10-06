#include "internal_sm.h"

#include "config.h"

IntTransition InternalSm::enter(const IntInputs& in, uint32_t nowMs) {
  IntTransition t{false, state_, state_, IntError::None};
  prevPower_ = in.power;
  powerFell_ = false;

  if (!in.power) {
    if (in.arm) {
      setState(IntState::Error, IntError::ArmNoPower, nowMs, t);
    } else if (in.fire) {
      setState(IntState::Error, IntError::FireNoPower, nowMs, t);
    } else {
      setState(IntState::NoPower, IntError::None, nowMs, t);
    }
  } else if (in.arm || in.fire) {
    setState(IntState::Error, IntError::SignalOnEntry, nowMs, t);
  } else {
    setState(IntState::Settling, IntError::None, nowMs, t);
  }
  // Entry always reports a transition so the log shows the starting state.
  t.changed = true;
  return t;
}

IntTransition InternalSm::reset(const IntInputs& in, uint32_t nowMs) {
  hasLast_ = false;
  lastError_ = IntError::None;
  return enter(in, nowMs);
}

IntTransition InternalSm::step(const IntInputs& in, uint32_t nowMs) {
  IntTransition t{false, state_, state_, IntError::None};

  const bool powerFellEdge = prevPower_ && !in.power;
  if (powerFellEdge) {
    powerFell_ = true;
    powerFellMs_ = nowMs;
  }
  prevPower_ = in.power;

  switch (state_) {
    case IntState::NoPower:
      if (!in.power) {
        // ARM/FIRE may lag POWER dropping; allow a short grace window.
        if (!inPowerLossGrace(nowMs)) {
          if (in.arm) {
            setState(IntState::Error, IntError::ArmNoPower, nowMs, t);
          } else if (in.fire) {
            setState(IntState::Error, IntError::FireNoPower, nowMs, t);
          }
        }
      } else if (in.arm) {
        setState(IntState::Error, IntError::ArmSettling, nowMs, t);
      } else if (in.fire) {
        setState(IntState::Error, IntError::FireSettling, nowMs, t);
      } else {
        setState(IntState::Settling, IntError::None, nowMs, t);
      }
      break;

    case IntState::Settling:
      if (!in.power) {
        setState(IntState::NoPower, IntError::None, nowMs, t);
      } else if (in.arm) {
        setState(IntState::Error, IntError::ArmSettling, nowMs, t);
      } else if (in.fire) {
        setState(IntState::Error, IntError::FireSettling, nowMs, t);
      } else if (nowMs - stateEnteredMs_ >= cfg::SETTLING_TIME_MS) {
        setState(IntState::Ready, IntError::None, nowMs, t);
      }
      break;

    case IntState::Ready:
      if (!in.power) {
        setState(IntState::NoPower, IntError::None, nowMs, t);
      } else if (in.fire) {
        // Includes ARM and FIRE arriving on the same tick.
        setState(IntState::Error, IntError::FireBeforeArm, nowMs, t);
      } else if (in.arm) {
        armedMs_ = nowMs;
        setState(IntState::Armed, IntError::None, nowMs, t);
      }
      break;

    case IntState::Armed:
      if (!in.power) {
        setState(IntState::NoPower, IntError::None, nowMs, t);
      } else if (in.fire) {
        armToFireMs_ = nowMs - armedMs_;
        if (armToFireMs_ < cfg::ARM_TO_FIRE_MIN_MS) {
          setState(IntState::Error, IntError::FireTooSoon, nowMs, t);
        } else {
          firedMs_ = nowMs;
          setState(IntState::Fired, IntError::None, nowMs, t);
        }
      } else if (!in.arm) {
        setState(IntState::Ready, IntError::None, nowMs, t);
      }
      break;

    case IntState::Fired:
      // Latched; ARM/FIRE activity is ignored while POWER is present.
      if (!in.power) {
        setState(IntState::NoPower, IntError::None, nowMs, t);
      }
      break;

    case IntState::Error:
      // Cleared by a POWER falling edge only, so an error raised without
      // power does not immediately clear itself.
      if (powerFellEdge) {
        setState(IntState::NoPower, IntError::None, nowMs, t);
      }
      break;
  }
  return t;
}

void InternalSm::outputs(uint32_t nowMs, bool& green, bool& red) const {
  const uint32_t elapsed = nowMs - stateEnteredMs_;
  green = false;
  red = false;
  switch (state_) {
    case IntState::NoPower:
    case IntState::Fired:
      break;
    case IntState::Settling:
      green = (elapsed / cfg::BLINK_SETTLING_MS) % 2 == 0;
      break;
    case IntState::Ready:
      green = (elapsed / cfg::BLINK_READY_MS) % 2 == 0;
      break;
    case IntState::Armed:
      red = true;
      break;
    case IntState::Error:
      green = red = (elapsed / cfg::BLINK_ERROR_MS) % 2 == 0;
      break;
  }
}

uint32_t InternalSm::settleRemainingS(uint32_t nowMs) const {
  if (state_ != IntState::Settling) return 0;
  const uint32_t elapsed = nowMs - stateEnteredMs_;
  if (elapsed >= cfg::SETTLING_TIME_MS) return 1;
  return (cfg::SETTLING_TIME_MS - elapsed + 999) / 1000;
}

void InternalSm::setState(IntState s, IntError e, uint32_t nowMs, IntTransition& t) {
  t.changed = (s != state_) || (e != error_);
  t.from = state_;
  t.to = s;
  t.error = e;
  state_ = s;
  error_ = e;
  stateEnteredMs_ = nowMs;
  if (s == IntState::Fired || s == IntState::Error) {
    hasLast_ = true;
    lastState_ = s;
    lastError_ = e;
    lastMs_ = nowMs;
  }
}

bool InternalSm::inPowerLossGrace(uint32_t nowMs) const {
  return powerFell_ && (nowMs - powerFellMs_) < cfg::POWER_LOSS_GRACE_MS;
}
