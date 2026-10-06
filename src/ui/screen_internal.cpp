// Internal Connector screen (SPEC §8.3)
//
//  y   0 +--------------------------------------------+
//      | NO PWR | SETTLE | READY | ARMED | FIRED | ERR|  chip strip   20
//   22 +--------------------------------------------+
//      | INT        READY (28pt)                    |  status banner 52
//      |          Waiting for ARM (14pt)            |
//   78 +--------------+--------------+--------------+
//      |    POWER     |     ARM      |     FIRE     |  inputs        54
//      |     ON       |     OFF      |     OFF      |
//  136 +----------------------+---------------------+
//      | GREEN            ON  | RED             OFF |  outputs       40
//  182 +----------------------+---------------------+
//      |        MENU          |       RESET         |  hold buttons  56
//  240 +----------------------+---------------------+
#include <cstdio>

#include "config.h"
#include "screens.h"
#include "ui.h"
#include "widgets.h"

namespace ui::internal_screen {
namespace {

constexpr const char* kChipNames[] = {"NO PWR", "SETTLE", "READY", "ARMED", "FIRED", "ERROR"};

ChipStrip chips;
StatusBanner banner;
Indicator inPower, inArm, inFire;
Indicator outGreen, outRed;
HoldButton btnMenu, btnReset;

void onMenu() { ui::show(Screen::Menu); }
void onReset() { io::send(io::Command{io::CmdType::Reset, LoadMode::Open}); }

}  // namespace

lv_obj_t* create() {
  lv_obj_t* scr = makeScreen();

  chips.create(scr, 0, 20, kChipNames, 6);
  banner.create(scr, 2, 22, 316, 52, "INT");

  inPower.create(scr, 2, 78, 104, 54, "POWER", color::Green, true, &lv_font_montserrat_16, &lv_font_montserrat_20);
  inArm.create(scr, 108, 78, 104, 54, "ARM", color::Orange, true, &lv_font_montserrat_16, &lv_font_montserrat_20);
  inFire.create(scr, 214, 78, 104, 54, "FIRE", color::Red, true, &lv_font_montserrat_16, &lv_font_montserrat_20);

  outGreen.create(scr, 2, 136, 157, 40, "GREEN", color::Green, false, &lv_font_montserrat_20, &lv_font_montserrat_20);
  outRed.create(scr, 161, 136, 157, 40, "RED", color::Red, false, &lv_font_montserrat_20, &lv_font_montserrat_20);

  btnMenu.create(scr, 2, 182, 157, 56, "MENU", 0x1565C0, onMenu, 0);
  btnReset.create(scr, 161, 182, 157, 56, "RESET", 0xC62828, onReset);

  return scr;
}

void update(const io::Snapshot& s, uint32_t nowMs) {
  char mainText[32];
  char detail[48];
  uint32_t bg = color::NoPower;
  uint32_t fg = color::White;

  switch (s.intState) {
    case IntState::NoPower:
      snprintf(mainText, sizeof mainText, "NO POWER");
      snprintf(detail, sizeof detail, "Apply POWER");
      bg = color::NoPower;
      fg = color::White;
      break;
    case IntState::Settling:
      snprintf(mainText, sizeof mainText, "POWER ON  %lu", (unsigned long)s.settleRemainingS);
      snprintf(detail, sizeof detail, "Settling - READY in %lu s", (unsigned long)s.settleRemainingS);
      bg = color::Settling;
      fg = color::Black;
      break;
    case IntState::Ready:
      snprintf(mainText, sizeof mainText, "READY");
      snprintf(detail, sizeof detail, "Waiting for ARM");
      bg = color::Ready;
      fg = color::Black;
      break;
    case IntState::Armed:
      snprintf(mainText, sizeof mainText, "ARMED");
      snprintf(detail, sizeof detail, s.timeInStateMs >= cfg::ARM_TO_FIRE_MIN_MS ? "Armed %.1f s - FIRE allowed"
                                                                               : "Armed %.1f s - wait before FIRE",
               s.timeInStateMs / 1000.0f);
      bg = color::Armed;
      fg = color::Black;
      break;
    case IntState::Fired:
      snprintf(mainText, sizeof mainText, "FIRED");
      snprintf(detail, sizeof detail, "Fired %lu s ago", (unsigned long)(s.timeInStateMs / 1000));
      break;
    case IntState::Error:
      snprintf(mainText, sizeof mainText, "ERROR");
      if (s.intError == IntError::FireTooSoon) {
        snprintf(detail, sizeof detail, "FIRE %lu ms AFTER ARM (< 1 s)", (unsigned long)s.armToFireMs);
      } else {
        snprintf(detail, sizeof detail, "%s", intErrorText(s.intError));
      }
      bg = ((nowMs / cfg::ERROR_FLASH_MS) % 2 == 0) ? color::Error : color::ErrorDim;
      fg = color::White;
      break;
  }

  // A FIRED/ERROR result that is no longer the live state (e.g. POWER was
  // removed) stays visible until RESET or MENU.
  const bool showLast = s.lastValid && s.lastState != s.intState;
  if (showLast) {
    if (s.lastState == IntState::Fired) {
      snprintf(detail, sizeof detail, "LAST RESULT: FIRED %lu s ago", (unsigned long)(s.lastAgeMs / 1000));
    } else {
      snprintf(detail, sizeof detail, "LAST: %s", intErrorText(s.lastError));
    }
  }

  const int active = int(s.intState);
  const int marked = showLast ? int(s.lastState) : -1;
  if (s.intState == IntState::Fired) {
    fired::show(banner, detail, nowMs);
    chips.set(active, fired::bg(nowMs), fired::fg(nowMs), marked);
  } else {
    fired::hide();
    banner.set(mainText, detail, bg, fg, showLast);
    chips.set(active, bg, fg, marked);
  }

  inPower.set(s.inPower);
  inArm.set(s.inArm);
  inFire.set(s.inFire);
  outGreen.set(s.outGreen);
  outRed.set(s.outRed);

  btnReset.setEnabled(s.intState == IntState::Fired || s.intState == IntState::Error || s.lastValid);
}

void tick(uint32_t nowMs) {
  btnMenu.update(nowMs);
  btnReset.update(nowMs);
}

}  // namespace ui::internal_screen
