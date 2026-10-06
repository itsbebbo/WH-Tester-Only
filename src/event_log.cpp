#include "event_log.h"

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "config.h"

namespace event_log {
namespace {

constexpr const char* kInputNames[3] = {"POWER", "ARM", "FIRE"};

uint32_t droppedLines = 0;

// Writes one complete line without ever blocking. When USB is connected but
// nothing on the host is reading, the CDC buffer fills; a blocking write would
// then stall the UI loop for up to 2 s per call. Lines that do not fit are
// dropped and counted instead.
void emit(uint32_t ms, const char* mode, const char* fmt, ...) {
  char line[160];
  int len = snprintf(line, sizeof line, "[%08lu] %-4s ", (unsigned long)ms, mode);
  va_list args;
  va_start(args, fmt);
  len += vsnprintf(line + len, sizeof line - len - 2, fmt, args);
  va_end(args);
  if (len > int(sizeof line) - 2) len = int(sizeof line) - 2;
  line[len++] = '\r';
  line[len++] = '\n';

  if (droppedLines > 0) {
    char note[48];
    const int noteLen = snprintf(note, sizeof note, "[%08lu] SYS  LOG_DROPPED %lu\r\n", (unsigned long)ms,
                                 (unsigned long)droppedLines);
    if (Serial.availableForWrite() < noteLen + len) {
      droppedLines++;
      return;
    }
    Serial.write(reinterpret_cast<const uint8_t*>(note), noteLen);
    droppedLines = 0;
  }

  if (Serial.availableForWrite() < len) {
    droppedLines++;
    return;
  }
  Serial.write(reinterpret_cast<const uint8_t*>(line), len);
}

}  // namespace

void begin() { Serial.setTxTimeoutMs(0); }

void boot(bool sensorOk) {
  emit(millis(), "SYS", "BOOT fw=%s ina219=%s addr=0x%02X", FW_VERSION, sensorOk ? "OK" : "NOT_FOUND",
       cfg::INA219_ADDR);
}

void write(const io::Event& e) {
  using io::EvtType;
  switch (e.type) {
    case EvtType::Screen:
      emit(e.ms, "SYS", "SCREEN %s", screenName(Screen(e.a)));
      break;

    case EvtType::InputEdge:
      emit(e.ms, "INT", "IN %s=%u", e.a < 3 ? kInputNames[e.a] : "?", e.b);
      break;

    case EvtType::IntState: {
      const IntState from = IntState(e.a);
      const IntState to = IntState(e.b);
      const IntError err = IntError(e.c);
      char extra[64] = "";
      int n = 0;
      if (err != IntError::None) n += snprintf(extra + n, sizeof extra - n, " code=%s", intErrorCode(err));
      if (err == IntError::FireTooSoon || to == IntState::Fired) {
        snprintf(extra + n, sizeof extra - n, " arm_to_fire=%ldms", (long)e.i);
      }
      if (e.d) {
        emit(e.ms, "INT", "STATE %s (entry)%s", intStateName(to), extra);
      } else {
        emit(e.ms, "INT", "STATE %s -> %s%s", intStateName(from), intStateName(to), extra);
        if (from == IntState::Armed && to == IntState::Ready) emit(e.ms, "INT", "DISARMED");
      }
      break;
    }

    case EvtType::IntReset:
      emit(e.ms, "INT", "RESET");
      break;

    case EvtType::ExtReset:
      emit(e.ms, "EXT", "RESET");
      break;

    case EvtType::LoadChange:
      emit(e.ms, "EXT", "LOAD %s", loadModeName(LoadMode(e.a)));
      break;

    case EvtType::ExtFired:
      emit(e.ms, "EXT", "FIRED i=%.3fA qualify=%ldms", e.f1, (long)e.i);
      break;

    case EvtType::PulseEnd:
      emit(e.ms, "EXT", "PULSE_END peak=%.3fA above_thr=%ldms", e.f1, (long)e.i);
      break;

    case EvtType::ExtFiredCleared:
      emit(e.ms, "EXT", "FIRED_CLEARED hold=%ldms (last result kept)", (long)e.i);
      break;

    case EvtType::SensorProbe:
      emit(e.ms, "EXT", "INA219 probe=%s addr=0x%02X", e.a ? "OK" : "FAIL", cfg::INA219_ADDR);
      break;

    case EvtType::SensorFault:
      emit(e.ms, "EXT", "SENSOR_FAULT i2c_err=%ld", (long)e.i);
      break;

    case EvtType::SensorRecovered:
      emit(e.ms, "EXT", "SENSOR_OK");
      break;
  }
}

}  // namespace event_log
