// Individual screens. Each creates its LVGL objects once and then refreshes
// them from the I/O snapshot.
#pragma once

#include <lvgl.h>

#include <cstdint>

#include "../io_task.h"

namespace ui {

namespace menu_screen {
lv_obj_t* create();
void update(const io::Snapshot& s, uint32_t nowMs);
void tick(uint32_t nowMs);
}  // namespace menu_screen

namespace internal_screen {
lv_obj_t* create();
void update(const io::Snapshot& s, uint32_t nowMs);
void tick(uint32_t nowMs);
}  // namespace internal_screen

namespace external_screen {
lv_obj_t* create();
void update(const io::Snapshot& s, uint32_t nowMs);
void tick(uint32_t nowMs);
}  // namespace external_screen

}  // namespace ui
