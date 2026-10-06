// Shared enums for the WH Board Tester. No hardware dependencies.
#pragma once

#include <cstdint>

enum class Screen : uint8_t { Menu, Internal, External };

// Internal-mode states (SPEC §6.1). Order matches the on-screen chip strip.
enum class IntState : uint8_t { NoPower, Settling, Ready, Armed, Fired, Error };

// Internal-mode error codes (SPEC §6.2)
enum class IntError : uint8_t {
  None,
  ArmNoPower,
  FireNoPower,
  ArmSettling,
  FireSettling,
  FireBeforeArm,
  FireTooSoon,
  SignalOnEntry,
};

enum class LoadMode : uint8_t { Open, Closed, OneOhm };

// External-mode status (SPEC §7.1)
enum class ExtStatus : uint8_t { Open, Closed, OneOhmMonitor, Switching, Fired, SensorFault };

inline const char* screenName(Screen s) {
  switch (s) {
    case Screen::Menu: return "MENU";
    case Screen::Internal: return "INTERNAL";
    case Screen::External: return "EXTERNAL";
  }
  return "?";
}

inline const char* intStateName(IntState s) {
  switch (s) {
    case IntState::NoPower: return "NO_POWER";
    case IntState::Settling: return "SETTLING";
    case IntState::Ready: return "READY";
    case IntState::Armed: return "ARMED";
    case IntState::Fired: return "FIRED";
    case IntState::Error: return "ERROR";
  }
  return "?";
}

inline const char* intErrorCode(IntError e) {
  switch (e) {
    case IntError::None: return "NONE";
    case IntError::ArmNoPower: return "E_ARM_NO_POWER";
    case IntError::FireNoPower: return "E_FIRE_NO_POWER";
    case IntError::ArmSettling: return "E_ARM_SETTLING";
    case IntError::FireSettling: return "E_FIRE_SETTLING";
    case IntError::FireBeforeArm: return "E_FIRE_BEFORE_ARM";
    case IntError::FireTooSoon: return "E_FIRE_TOO_SOON";
    case IntError::SignalOnEntry: return "E_SIGNAL_ON_ENTRY";
  }
  return "?";
}

// Short operator-facing reason text (fits the status banner detail line)
inline const char* intErrorText(IntError e) {
  switch (e) {
    case IntError::None: return "";
    case IntError::ArmNoPower: return "ARM WITHOUT POWER";
    case IntError::FireNoPower: return "FIRE WITHOUT POWER";
    case IntError::ArmSettling: return "ARM DURING SETTLING";
    case IntError::FireSettling: return "FIRE DURING SETTLING";
    case IntError::FireBeforeArm: return "FIRE BEFORE ARM";
    case IntError::FireTooSoon: return "FIRE < 1 s AFTER ARM";
    case IntError::SignalOnEntry: return "ARM/FIRE ACTIVE ON ENTRY";
  }
  return "";
}

inline const char* loadModeName(LoadMode m) {
  switch (m) {
    case LoadMode::Open: return "OPEN";
    case LoadMode::Closed: return "SHORT";
    case LoadMode::OneOhm: return "1R";
  }
  return "?";
}
