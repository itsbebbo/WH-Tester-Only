// Non-blocking speaker sounds (SPEC §9): explosion sample on FIRED, beeps on
// ERROR, click on touch. Call from the UI task only.
#pragma once

#include <cstdint>

namespace beeper {

void begin();
void playFired();
void playError();
void click();
void update(uint32_t nowMs);

}  // namespace beeper
