// WH Board Tester — M5Stack CoreS3 SE
// See SPEC.md. Real-time I/O runs in io_task (core 0); this loop (core 1)
// handles the LVGL interface, speaker and serial log.
#include <M5Unified.h>

#include "beeper.h"
#include "config.h"
#include "event_log.h"
#include "io_task.h"
#include "pins.h"
#include "ui/ui.h"

void setup() {
  // SPEC §3.6: drive outputs and relays LOW before anything else.
  for (uint8_t pin : {pins::GREEN_OUT, pins::RED_OUT, pins::LOAD_TEST, pins::LOAD_SELECT}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  for (uint8_t pin : {pins::POWER_IN, pins::ARM_IN, pins::FIRE_IN}) {
    pinMode(pin, INPUT);  // external divider provides the pull-down
  }

  auto m5cfg = M5.config();
  m5cfg.serial_baudrate = 115200;
  m5cfg.internal_mic = false;
  m5cfg.output_power = true;  // 5 V on the Grove ports for the INA219
  M5.begin(m5cfg);
  event_log::begin();
  M5.Display.setBrightness(cfg::DISPLAY_BRIGHTNESS);
  M5.Ex_I2C.begin();

  beeper::begin();

  const bool sensorOk = io::start();
  event_log::boot(sensorOk);

  ui::begin();
}

void loop() {
  M5.update();  // touch
  const uint32_t nowMs = millis();

  io::Event e;
  while (io::pollEvent(e)) {
    event_log::write(e);
    if (e.type == io::EvtType::ExtFired ||
        (e.type == io::EvtType::IntState && IntState(e.b) == IntState::Fired)) {
      beeper::playFired();
    } else if (e.type == io::EvtType::IntState && IntState(e.b) == IntState::Error) {
      beeper::playError();
    }
  }

  beeper::update(nowMs);
  ui::update(nowMs);
  delay(2);
}
