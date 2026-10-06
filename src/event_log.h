// Serial event log over USB CDC (SPEC §10). Call from the UI task only.
// Never blocks: lines are dropped (and counted) if the host is not reading.
#pragma once

#include "io_task.h"

namespace event_log {

void begin();  // call after M5.begin() has started Serial
void boot(bool sensorOk);
void write(const io::Event& e);

}  // namespace event_log
