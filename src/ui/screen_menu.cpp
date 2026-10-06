// Main menu (SPEC §5.2)
#include "config.h"
#include "screens.h"
#include "ui.h"
#include "widgets.h"

namespace ui::menu_screen {
namespace {

TapButton btnInternal;
TapButton btnExternal;
lv_obj_t* sensorLabel = nullptr;

void onInternal(intptr_t) { ui::show(Screen::Internal); }
void onExternal(intptr_t) { ui::show(Screen::External); }

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = makeScreen();

  lv_obj_t* title = makeLabel(scr, &lv_font_montserrat_24, color::White, "WH BOARD TESTER");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

  lv_obj_t* ver = makeLabel(scr, &lv_font_montserrat_14, color::OffText, "Version " FW_VERSION);
  lv_obj_align(ver, LV_ALIGN_TOP_MID, 0, 36);

  btnInternal.create(scr, 10, 60, 300, 74, "INTERNAL\nCONNECTOR", &lv_font_montserrat_24, onInternal, 0);
  btnInternal.setColors(0x0D47A1, color::White);

  btnExternal.create(scr, 10, 142, 300, 74, "EXTERNAL\nCONNECTOR", &lv_font_montserrat_24, onExternal, 0);
  btnExternal.setColors(0x6A1B9A, color::White);

  sensorLabel = makeLabel(scr, &lv_font_montserrat_16, color::OffText, "Current Sensor --");
  lv_obj_align(sensorLabel, LV_ALIGN_BOTTOM_MID, 0, -2);

  return scr;
}

void update(const io::Snapshot& s, uint32_t) {
  setText(sensorLabel, s.sensorOk ? "Current Sensor OK" : "Current Sensor Not Found");
  setTextColor(sensorLabel, s.sensorOk ? color::Green : color::Red);
}

void tick(uint32_t) {}

}  // namespace ui::menu_screen
