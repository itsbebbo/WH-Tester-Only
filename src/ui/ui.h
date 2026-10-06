// Screen manager. Runs in the Arduino loop() task (core 1).
#pragma once

#include <cstdint>

#include "tester_types.h"

namespace ui {

void begin();

// Switches the visible screen and tells the I/O task.
void show(Screen s);

// Call every loop(): hold buttons, snapshot refresh and lv_timer_handler().
void update(uint32_t nowMs);

}  // namespace ui
