// Real-time I/O task (SPEC §4.1): owns every GPIO and the Port A I2C bus.
// Runs every 1 ms on core 0, independent of screen rendering.
#pragma once

#include <cstdint>

#include "tester_types.h"

namespace io {

enum class CmdType : uint8_t { EnterMenu, EnterInternal, EnterExternal, Reset, SetLoad };

struct Command {
  CmdType type;
  LoadMode load;
};

enum class EvtType : uint8_t {
  Screen,          // a = Screen
  InputEdge,       // a = 0 POWER / 1 ARM / 2 FIRE, b = level
  IntState,        // a = from, b = to, c = IntError, d = 1 for entry/reset, i = arm->fire ms
  IntReset,
  ExtReset,
  LoadChange,      // a = LoadMode
  ExtFired,        // f1 = qualifying current
  PulseEnd,        // f1 = pulse peak, i = ms above threshold
  ExtFiredCleared, // i = hold time ms
  SensorProbe,     // a = ok
  SensorFault,     // i = consecutive failures
  SensorRecovered,
};

struct Event {
  EvtType type;
  uint32_t ms;
  uint8_t a, b, c, d;
  int32_t i;
  float f1;
};

// Copy of everything the UI needs, published every tick.
struct Snapshot {
  uint32_t nowMs;
  Screen screen;

  // Debounced inputs and actual driven outputs
  bool inPower, inArm, inFire;
  bool outGreen, outRed;

  // Internal mode
  IntState intState;
  IntError intError;
  uint32_t armToFireMs;
  uint32_t settleRemainingS;
  uint32_t timeInStateMs;
  bool lastValid;
  IntState lastState;
  IntError lastError;
  uint32_t lastAgeMs;

  // External mode
  ExtStatus extStatus;
  LoadMode loadSelected;
  bool extFired;
  uint32_t extHoldRemainingMs;  // countdown after current is lost, 0 otherwise
  bool extLastFired;            // FIRED has cleared; LAST RESULT shown until RESET/MENU
  uint32_t extFiredAgeMs;
  float currentA;       // averaged for display
  float firedPeakA;     // peak since RESET
  bool sensorOk;
};

// Configures pins, probes the INA219 and starts the task. Returns the probe result.
bool start();

void send(const Command& cmd);
void getSnapshot(Snapshot& out);
bool pollEvent(Event& out);

}  // namespace io
