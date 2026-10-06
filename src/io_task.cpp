#include "io_task.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>

#include "config.h"
#include "fire_detect.h"
#include "ina219.h"
#include "internal_sm.h"
#include "pins.h"

namespace io {
namespace {

// ---------------------------------------------------------------------------
// Debounced digital input
// ---------------------------------------------------------------------------
struct Debounced {
  uint8_t pin;
  bool stable;
  bool candidate;
  uint32_t candidateSinceMs;

  void begin(uint8_t p, uint32_t nowMs) {
    pin = p;
    stable = candidate = digitalRead(pin) == HIGH;
    candidateSinceMs = nowMs;
  }

  // Returns true when the stable level changes.
  bool update(uint32_t nowMs) {
    const bool raw = digitalRead(pin) == HIGH;
    if (raw != candidate) {
      candidate = raw;
      candidateSinceMs = nowMs;
    } else if (candidate != stable && nowMs - candidateSinceMs >= cfg::INPUT_DEBOUNCE_MS) {
      stable = candidate;
      return true;
    }
    return false;
  }
};

enum class RelayPhase : uint8_t { Idle, Release, Settle, Blanking };

// ---------------------------------------------------------------------------
// Task state (only touched from the I/O task after start())
// ---------------------------------------------------------------------------
QueueHandle_t cmdQueue;
QueueHandle_t evtQueue;
portMUX_TYPE snapMux = portMUX_INITIALIZER_UNLOCKED;
Snapshot shared{};

Screen screen = Screen::Menu;
Debounced inputs[3];
bool outGreen = false, outRed = false;

InternalSm sm;

Ina219 ina;
FireDetector detector;
bool sensorProbeOk = false;
bool lastReadFailed = false;
uint32_t lastReadAttemptMs = 0;
uint32_t extFiredMs = 0;
float avgBuf[cfg::CURRENT_AVG_SAMPLES] = {};
uint16_t avgIdx = 0;
uint16_t avgCount = 0;
float avgSum = 0;

LoadMode loadSelected = LoadMode::Open;
LoadMode loadTarget = LoadMode::Open;
RelayPhase relayPhase = RelayPhase::Idle;
uint32_t relayPhaseUntilMs = 0;

// ---------------------------------------------------------------------------
void postEvent(EvtType type, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0,
               int32_t i = 0, float f1 = 0) {
  Event e{type, millis(), a, b, c, d, i, f1};
  xQueueSend(evtQueue, &e, 0);  // drop if the UI is behind; never block
}

void postTransition(const IntTransition& t, bool entry) {
  if (!t.changed) return;
  postEvent(EvtType::IntState, uint8_t(t.from), uint8_t(t.to), uint8_t(t.error), entry ? 1 : 0,
            int32_t(sm.armToFireMs()));
}

void writeOutputs(bool green, bool red) {
  if (green != outGreen) {
    outGreen = green;
    digitalWrite(pins::GREEN_OUT, green ? HIGH : LOW);
  }
  if (red != outRed) {
    outRed = red;
    digitalWrite(pins::RED_OUT, red ? HIGH : LOW);
  }
}

void writeRelayTest(bool on) { digitalWrite(pins::LOAD_TEST, on ? HIGH : LOW); }

void writeRelaySelect(bool on) { digitalWrite(pins::LOAD_SELECT, on ? HIGH : LOW); }

IntInputs currentInputs() { return IntInputs{inputs[0].stable, inputs[1].stable, inputs[2].stable}; }

// Break-before-make load change (SPEC §7.3)
void startLoadChange(LoadMode target, uint32_t nowMs) {
  loadSelected = target;
  loadTarget = target;
  writeRelayTest(false);
  relayPhase = RelayPhase::Release;
  relayPhaseUntilMs = nowMs + cfg::RELAY_RELEASE_MS;
  detector.clearQualification();
}

void updateRelays(uint32_t nowMs) {
  if (relayPhase == RelayPhase::Idle || nowMs < relayPhaseUntilMs) return;
  switch (relayPhase) {
    case RelayPhase::Release:
      writeRelaySelect(loadTarget == LoadMode::OneOhm);
      relayPhase = RelayPhase::Settle;
      relayPhaseUntilMs = nowMs + cfg::RELAY_SETTLE_MS;
      break;
    case RelayPhase::Settle:
      if (loadTarget != LoadMode::Open) writeRelayTest(true);
      relayPhase = RelayPhase::Blanking;
      relayPhaseUntilMs = nowMs + cfg::DETECT_BLANKING_MS;
      break;
    case RelayPhase::Blanking:
      relayPhase = RelayPhase::Idle;
      break;
    case RelayPhase::Idle:
      break;
  }
}

void resetCurrentAverage() {
  avgIdx = avgCount = 0;
  avgSum = 0;
}

void pushCurrentAverage(float a) {
  if (avgCount == cfg::CURRENT_AVG_SAMPLES) {
    avgSum -= avgBuf[avgIdx];
  } else {
    avgCount++;
  }
  avgBuf[avgIdx] = a;
  avgSum += a;
  avgIdx = (avgIdx + 1) % cfg::CURRENT_AVG_SAMPLES;
}

void handleCommand(const Command& c, uint32_t nowMs) {
  switch (c.type) {
    case CmdType::EnterMenu:
      if (screen == Screen::External || loadTarget != LoadMode::Open) {
        startLoadChange(LoadMode::Open, nowMs);
      }
      screen = Screen::Menu;
      writeOutputs(false, false);
      postEvent(EvtType::Screen, uint8_t(screen));
      break;

    case CmdType::EnterInternal:
      if (loadTarget != LoadMode::Open) startLoadChange(LoadMode::Open, nowMs);
      screen = Screen::Internal;
      postEvent(EvtType::Screen, uint8_t(screen));
      postTransition(sm.reset(currentInputs(), nowMs), true);
      break;

    case CmdType::EnterExternal:
      writeOutputs(false, false);
      screen = Screen::External;
      postEvent(EvtType::Screen, uint8_t(screen));
      // Default selection is the 1 ohm load (break-before-make sequence applies).
      startLoadChange(cfg::EXTERNAL_DEFAULT_LOAD, nowMs);
      postEvent(EvtType::LoadChange, uint8_t(cfg::EXTERNAL_DEFAULT_LOAD));
      detector.reset();
      resetCurrentAverage();
      sensorProbeOk = ina.begin(cfg::INA219_ADDR);
      postEvent(EvtType::SensorProbe, sensorProbeOk ? 1 : 0);
      break;

    case CmdType::Reset:
      if (screen == Screen::Internal) {
        postEvent(EvtType::IntReset);
        postTransition(sm.reset(currentInputs(), nowMs), true);
      } else if (screen == Screen::External) {
        postEvent(EvtType::ExtReset);
        detector.reset();
      }
      break;

    case CmdType::SetLoad:
      if (screen == Screen::External && c.load != loadSelected) {
        startLoadChange(c.load, nowMs);
        postEvent(EvtType::LoadChange, uint8_t(c.load));
      }
      break;
  }
}

void runInternal(uint32_t nowMs, const bool edges[3]) {
  for (uint8_t k = 0; k < 3; k++) {
    if (edges[k]) postEvent(EvtType::InputEdge, k, inputs[k].stable ? 1 : 0);
  }
  postTransition(sm.step(currentInputs(), nowMs), false);

  bool g, r;
  sm.outputs(nowMs, g, r);
  writeOutputs(g, r);
}

void runExternal(uint32_t nowMs, uint64_t nowUs) {
  // While the sensor is faulted, retry slowly so a missing device cannot
  // stall the 1 ms loop.
  if (lastReadFailed && detector.sensorFault() && nowMs - lastReadAttemptMs < cfg::SENSOR_RETRY_MS) {
    return;
  }
  lastReadAttemptMs = nowMs;

  float amps = 0;
  const bool ok = ina.readCurrent(amps);
  lastReadFailed = !ok;
  if (ok) pushCurrentAverage(amps);

  const bool detectEnabled = relayPhase == RelayPhase::Idle && loadTarget == LoadMode::OneOhm;
  const FireSampleResult r = detector.sample(ok, amps, nowUs, detectEnabled);

  if (r.faultNow) {
    postEvent(EvtType::SensorFault, 0, 0, 0, 0, detector.failCount());
    resetCurrentAverage();
  }
  if (r.recoveredNow) {
    sensorProbeOk = ina.begin(cfg::INA219_ADDR);  // re-apply config after a replug
    postEvent(EvtType::SensorRecovered);
  }
  if (r.firedNow) {
    extFiredMs = nowMs;
    postEvent(EvtType::ExtFired, 0, 0, 0, 0, int32_t(cfg::FIRE_QUALIFY_MS), detector.firedCurrentA());
  }
  if (r.pulseEnded) {
    postEvent(EvtType::PulseEnd, 0, 0, 0, 0, int32_t(detector.lastPulseMs()), detector.lastPulsePeakA());
  }
  if (r.firedCleared) {
    postEvent(EvtType::ExtFiredCleared, 0, 0, 0, 0, int32_t(cfg::EXT_FIRED_HOLD_MS));
  }
}

ExtStatus externalStatus() {
  if (detector.fired()) return ExtStatus::Fired;
  if (loadSelected == LoadMode::OneOhm && detector.sensorFault()) return ExtStatus::SensorFault;
  if (relayPhase != RelayPhase::Idle) return ExtStatus::Switching;
  switch (loadTarget) {
    case LoadMode::Open: return ExtStatus::Open;
    case LoadMode::Closed: return ExtStatus::Closed;
    case LoadMode::OneOhm: return ExtStatus::OneOhmMonitor;
  }
  return ExtStatus::Open;
}

void publish(uint32_t nowMs, uint64_t nowUs) {
  Snapshot s{};
  s.nowMs = nowMs;
  s.screen = screen;
  s.inPower = inputs[0].stable;
  s.inArm = inputs[1].stable;
  s.inFire = inputs[2].stable;
  s.outGreen = outGreen;
  s.outRed = outRed;

  s.intState = sm.state();
  s.intError = sm.error();
  s.armToFireMs = sm.armToFireMs();
  s.settleRemainingS = sm.settleRemainingS(nowMs);
  s.timeInStateMs = nowMs - sm.stateEnteredMs();
  s.lastValid = sm.hasLastResult();
  s.lastState = sm.lastResult();
  s.lastError = sm.lastError();
  s.lastAgeMs = nowMs - sm.lastResultMs();

  s.extStatus = externalStatus();
  s.loadSelected = loadSelected;
  s.extFired = detector.fired();
  s.extHoldRemainingMs = detector.holdRemainingMs(nowUs);
  s.extLastFired = detector.hasLastFired();
  s.extFiredAgeMs = nowMs - extFiredMs;
  s.currentA = avgCount ? avgSum / avgCount : 0;
  s.firedPeakA = detector.peakA();
  s.sensorOk = sensorProbeOk && !detector.sensorFault();

  portENTER_CRITICAL(&snapMux);
  shared = s;
  portEXIT_CRITICAL(&snapMux);
}

void ioTask(void*) {
  if (esp_task_wdt_add(nullptr) != ESP_OK) {
    const esp_task_wdt_config_t wdt = {cfg::IO_TASK_WDT_MS, 0, true};
    esp_task_wdt_init(&wdt);
    esp_task_wdt_add(nullptr);
  }

  const TickType_t period = pdMS_TO_TICKS(1) > 0 ? pdMS_TO_TICKS(1) : 1;
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    xTaskDelayUntil(&lastWake, period);
    esp_task_wdt_reset();

    const uint32_t nowMs = millis();
    const uint64_t nowUs = esp_timer_get_time();

    Command c;
    while (xQueueReceive(cmdQueue, &c, 0) == pdTRUE) handleCommand(c, nowMs);

    bool edges[3];
    for (int k = 0; k < 3; k++) edges[k] = inputs[k].update(nowMs);

    updateRelays(nowMs);

    switch (screen) {
      case Screen::Internal:
        runInternal(nowMs, edges);
        break;
      case Screen::External:
        runExternal(nowMs, nowUs);
        break;
      case Screen::Menu:
        break;
    }

    publish(nowMs, nowUs);
  }
}

}  // namespace

bool start() {
  // Outputs are already LOW from setup(); re-assert so cached state agrees.
  outGreen = outRed = false;
  digitalWrite(pins::GREEN_OUT, LOW);
  digitalWrite(pins::RED_OUT, LOW);
  writeRelayTest(false);
  writeRelaySelect(false);

  const uint32_t nowMs = millis();
  inputs[0].begin(pins::POWER_IN, nowMs);
  inputs[1].begin(pins::ARM_IN, nowMs);
  inputs[2].begin(pins::FIRE_IN, nowMs);

  sensorProbeOk = ina.begin(cfg::INA219_ADDR);

  cmdQueue = xQueueCreate(16, sizeof(Command));
  evtQueue = xQueueCreate(64, sizeof(Event));
  publish(nowMs, esp_timer_get_time());

  xTaskCreatePinnedToCore(ioTask, "io", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 0);
  return sensorProbeOk;
}

void send(const Command& cmd) { xQueueSend(cmdQueue, &cmd, pdMS_TO_TICKS(10)); }

void getSnapshot(Snapshot& out) {
  portENTER_CRITICAL(&snapMux);
  out = shared;
  portEXIT_CRITICAL(&snapMux);
}

bool pollEvent(Event& out) { return xQueueReceive(evtQueue, &out, 0) == pdTRUE; }

}  // namespace io
