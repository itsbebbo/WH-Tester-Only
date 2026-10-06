// External Connector screen (SPEC §8.5)
//
//  y   0 +--------------------------------------------+
//      | OPEN | CLOSED | 1 OHM | FIRED | FAULT        |  chip strip    20
//   22 +--------------------------------------------+
//      | EXT       1 OHM LOAD (28pt)                |  status banner 52
//      |     FIRES at >= 1.20 A for 5 ms            |
//   78 +--------------------------------------------+
//      | I =                              0.842 A   |  current (32pt)  40
//  122 [==========================|=================]  bar 0..2 A      14
//  140 +--------------+--------------+--------------+
//      |     OPEN     |    CLOSED    |    1 OHM     |  load select     40
//  184 +----------------------+---------------------+
//      |        MENU          |       RESET         |  hold buttons    54
//  240 +----------------------+---------------------+
#include <cstdio>

#include "config.h"
#include "screens.h"
#include "ui.h"
#include "widgets.h"

namespace ui::external_screen {
namespace {

constexpr const char* kChipNames[] = {"OPEN", "CLOSED", "1 OHM", "FIRED", "FAULT"};
constexpr int32_t kBarX = 2;
constexpr int32_t kBarW = 316;

ChipStrip chips;
StatusBanner banner;
lv_obj_t* currentBox = nullptr;
lv_obj_t* currentTag = nullptr;
lv_obj_t* currentValue = nullptr;
lv_obj_t* bar = nullptr;
TapButton loadButtons[3];
HoldButton btnMenu, btnReset;
bool lastBarAbove = false;

void onLoad(intptr_t mode) { io::send(io::Command{io::CmdType::SetLoad, LoadMode(mode)}); }
void onMenu() { ui::show(Screen::Menu); }
void onReset() { io::send(io::Command{io::CmdType::Reset, LoadMode::Open}); }

uint32_t loadColor(LoadMode m) {
  switch (m) {
    case LoadMode::Open: return color::Blue;
    case LoadMode::Closed: return color::Magenta;
    case LoadMode::OneOhm: return color::Cyan;
  }
  return color::Off;
}

uint32_t loadTextColor(LoadMode m) { return m == LoadMode::Open ? color::White : color::Black; }

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = makeScreen();

  chips.create(scr, 0, 20, kChipNames, 5);
  banner.create(scr, 2, 22, 316, 52, "EXT");

  currentBox = makeBox(scr, 2, 78, 316, 40, color::Panel);
  currentTag = makeLabel(currentBox, &lv_font_montserrat_20, color::OffText, "I =");
  lv_obj_align(currentTag, LV_ALIGN_LEFT_MID, 10, 0);
  currentValue = makeLabel(currentBox, &lv_font_montserrat_32, color::White, "-.--- A");
  lv_obj_align(currentValue, LV_ALIGN_RIGHT_MID, -10, 0);

  bar = lv_bar_create(scr);
  lv_obj_remove_style_all(bar);
  lv_obj_set_pos(bar, kBarX, 122);
  lv_obj_set_size(bar, kBarW, 14);
  lv_bar_set_range(bar, 0, int32_t(cfg::CURRENT_BAR_MAX_A * 1000));
  lv_obj_set_style_bg_color(bar, lv_color_hex(color::Off), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(color::Cyan), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);

  // Threshold marker at FIRE_CURRENT_A
  const int32_t markerX = kBarX + int32_t(kBarW * (cfg::FIRE_CURRENT_A / cfg::CURRENT_BAR_MAX_A)) - 1;
  makeBox(scr, markerX, 119, 3, 20, color::White, 0);

  loadButtons[0].create(scr, 2, 140, 104, 40, "OPEN", &lv_font_montserrat_20, onLoad, intptr_t(LoadMode::Open));
  loadButtons[1].create(scr, 108, 140, 104, 40, "CLOSED", &lv_font_montserrat_20, onLoad, intptr_t(LoadMode::Closed));
  loadButtons[2].create(scr, 214, 140, 104, 40, "1 OHM", &lv_font_montserrat_20, onLoad, intptr_t(LoadMode::OneOhm));

  btnMenu.create(scr, 2, 184, 157, 54, "MENU", 0x1565C0, onMenu, 0);
  btnReset.create(scr, 161, 184, 157, 54, "RESET", 0xC62828, onReset);

  return scr;
}

void update(const io::Snapshot& s, uint32_t nowMs) {
  char detail[48];
  const int loadIdx = int(s.loadSelected);

  // After FIRED clears (EXT_FIRED_HOLD_MS after the current is lost) the
  // result stays visible, as on the Internal screen, until RESET or MENU.
  constexpr int kFiredChip = 3;
  const bool showLast = s.extLastFired && s.extStatus != ExtStatus::Fired;
  char lastText[48];
  if (showLast) {
    snprintf(lastText, sizeof lastText, "LAST RESULT: FIRED %lu s ago", (unsigned long)(s.extFiredAgeMs / 1000));
  }
  const int marked = showLast ? kFiredChip : -1;

  switch (s.extStatus) {
    case ExtStatus::Open:
      banner.set("OPEN CCT", showLast ? lastText : "Monitoring inactive", color::Blue, color::White, showLast);
      chips.set(0, color::Blue, color::White, marked);
      break;
    case ExtStatus::Closed:
      banner.set("CLOSED CCT", showLast ? lastText : "Monitoring inactive", color::Magenta, color::Black, showLast);
      chips.set(1, color::Magenta, color::Black, marked);
      break;
    case ExtStatus::OneOhmMonitor:
      snprintf(detail, sizeof detail, "FIRES at >= %.2f A for %lu ms", cfg::FIRE_CURRENT_A,
               (unsigned long)cfg::FIRE_QUALIFY_MS);
      banner.set("1 OHM LOAD", showLast ? lastText : detail, color::Cyan, color::Black, showLast);
      chips.set(2, color::Cyan, color::Black, marked);
      break;
    case ExtStatus::Switching:
      banner.set("SWITCHING", "Relays changing...", color::NoPower, color::White);
      chips.set(-1, color::Off, color::OffText, loadIdx);
      break;
    case ExtStatus::SensorFault:
      banner.set("SENSOR FAULT", "Check INA219 on Port A", color::Error, color::White);
      chips.set(4, color::Error, color::White, loadIdx);
      break;
    case ExtStatus::Fired:
      if (s.extHoldRemainingMs > 0) {
        snprintf(detail, sizeof detail, "Peak %.2f A - clears in %lu s", s.firedPeakA,
                 (unsigned long)((s.extHoldRemainingMs + 999) / 1000));
      } else {
        snprintf(detail, sizeof detail, "Fired %lu s ago - peak %.2f A", (unsigned long)(s.extFiredAgeMs / 1000),
                 s.firedPeakA);
      }
      chips.set(3, fired::bg(nowMs), fired::fg(nowMs), loadIdx);
      break;
  }
  if (s.extStatus == ExtStatus::Fired) {
    fired::show(banner, detail, nowMs);
  } else {
    fired::hide();
  }

  // Current readout and bar. Outside 1 ohm mode the live value is still shown
  // but dimmed; the banner says "Monitoring inactive".
  const bool monitoring = s.loadSelected == LoadMode::OneOhm;
  if (s.sensorOk) {
    char value[16];
    snprintf(value, sizeof value, "%.3f A", s.currentA);
    setText(currentValue, value);
    const bool aboveThreshold = s.currentA >= cfg::FIRE_CURRENT_A;
    setTextColor(currentValue, !monitoring ? color::OffText : aboveThreshold ? color::Red : color::White);
  } else {
    setText(currentValue, "-.--- A");
    setTextColor(currentValue, color::OffText);
  }
  int32_t barValue = s.sensorOk ? int32_t(s.currentA * 1000) : 0;
  if (barValue < 0) barValue = 0;
  if (lv_bar_get_value(bar) != barValue) lv_bar_set_value(bar, barValue, LV_ANIM_OFF);
  const bool above = s.sensorOk && s.currentA >= cfg::FIRE_CURRENT_A;
  if (above != lastBarAbove) {
    lv_obj_set_style_bg_color(bar, lv_color_hex(above ? color::Red : color::Cyan), LV_PART_INDICATOR);
    lastBarAbove = above;
  }

  for (int i = 0; i < 3; i++) {
    const LoadMode m = LoadMode(i);
    if (m == s.loadSelected) {
      loadButtons[i].setColors(loadColor(m), loadTextColor(m));
    } else {
      loadButtons[i].setColors(color::ButtonBg, color::White);
    }
  }

  btnReset.setEnabled(s.extFired || s.extLastFired);
}

void tick(uint32_t nowMs) {
  btnMenu.update(nowMs);
  btnReset.update(nowMs);
}

}  // namespace ui::external_screen
