// Reusable LVGL widgets for the tester screens (SPEC §8).
// All setters are cheap to call every refresh: LVGL is only touched when a
// value actually changes, which keeps redraws (and SPI traffic) minimal.
#pragma once

#include <lvgl.h>

#include <cstdint>

#include "config.h"

namespace ui {

// ---- Palette (SPEC §8.2) ---------------------------------------------------
namespace color {
constexpr uint32_t Black = 0x000000;
constexpr uint32_t White = 0xFFFFFF;
constexpr uint32_t Off = 0x303030;
constexpr uint32_t OffText = 0x9A9A9A;
constexpr uint32_t Panel = 0x141414;
constexpr uint32_t ButtonBg = 0x202020;
constexpr uint32_t ButtonBorder = 0x808080;

constexpr uint32_t Green = 0x00FF00;
constexpr uint32_t Orange = 0xFF8C00;
constexpr uint32_t Red = 0xFF0000;
constexpr uint32_t Yellow = 0xFFFF00;
constexpr uint32_t Cyan = 0x00E5FF;
constexpr uint32_t Blue = 0x0060FF;
constexpr uint32_t Magenta = 0xFF00FF;

constexpr uint32_t NoPower = 0x606060;
constexpr uint32_t Settling = 0xFFB000;
constexpr uint32_t Ready = 0x00C000;
constexpr uint32_t Armed = 0xFF6000;
constexpr uint32_t Error = 0xC00000;
constexpr uint32_t ErrorDim = 0x500000;
}  // namespace color

// ---- Helpers ---------------------------------------------------------------
lv_obj_t* makeScreen();
lv_obj_t* makeBox(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg,
                  int32_t radius = 6);
lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t textColor, const char* text);

void setText(lv_obj_t* label, const char* text);
void setBg(lv_obj_t* obj, uint32_t rgb);
void setTextColor(lv_obj_t* obj, uint32_t rgb);

// ---- Indicator tile: bright colour when ON, dark grey when OFF -------------
class Indicator {
 public:
  // stacked = title above value; otherwise title left, value right.
  void create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* title,
              uint32_t onColor, bool stacked, const lv_font_t* titleFont, const lv_font_t* valueFont);
  void set(bool on, const char* onText = "ON", const char* offText = "OFF");

 private:
  lv_obj_t* box_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* value_ = nullptr;
  uint32_t onColor_ = 0;
};

// ---- Row of state chips; current state filled, "last result" outlined ------
class ChipStrip {
 public:
  static constexpr uint8_t kMax = 6;
  void create(lv_obj_t* parent, int32_t y, int32_t h, const char* const names[], uint8_t count);
  void set(int active, uint32_t activeBg, uint32_t activeFg, int marked);

 private:
  lv_obj_t* chips_[kMax] = {};
  lv_obj_t* labels_[kMax] = {};
  uint8_t count_ = 0;
  int lastMarked_ = -2;
};

// ---- Large status banner with detail line ---------------------------------
class StatusBanner {
 public:
  void create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* tag);
  // detailAlert: detail drawn as yellow-on-black strip (used for LAST RESULT)
  void set(const char* mainText, const char* detailText, uint32_t bg, uint32_t fg, bool detailAlert = false);

 private:
  lv_obj_t* box_ = nullptr;
  lv_obj_t* tag_ = nullptr;
  lv_obj_t* main_ = nullptr;
  lv_obj_t* detail_ = nullptr;
  int8_t lastAlert_ = -1;
};

// ---- Single-tap button ------------------------------------------------------
class TapButton {
 public:
  using Callback = void (*)(intptr_t arg);
  void create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text,
              const lv_font_t* font, Callback cb, intptr_t arg);
  void setColors(uint32_t bg, uint32_t fg);

 private:
  static void onEvent(lv_event_t* e);
  lv_obj_t* btn_ = nullptr;
  lv_obj_t* label_ = nullptr;
  Callback cb_ = nullptr;
  intptr_t arg_ = 0;
};

// ---- Press-and-hold button (MENU / RESET) -----------------------------------
// Fills with colour while held; triggers once after holdMs (0 = on press).
class HoldButton {
 public:
  using Callback = void (*)();
  void create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text,
              uint32_t accent, Callback cb, uint32_t holdMs = cfg::HOLD_BUTTON_MS);
  void setEnabled(bool enabled);
  void update(uint32_t nowMs);

 private:
  static void onEvent(lv_event_t* e);
  lv_obj_t* btn_ = nullptr;
  lv_obj_t* fill_ = nullptr;
  lv_obj_t* label_ = nullptr;
  lv_obj_t* hint_ = nullptr;
  int32_t width_ = 0;
  uint32_t holdMs_ = 0;
  Callback cb_ = nullptr;
  bool enabled_ = true;
  bool pressing_ = false;
  bool triggered_ = false;
  uint32_t pressStartMs_ = 0;
  int32_t lastFillW_ = -1;
};

// ---- Shared FIRED indication (SPEC §8.4), identical on both screens ---------
namespace fired {
void begin();                              // creates the full-screen border on the top layer
bool phase(uint32_t nowMs);                // flash phase (true = red background)
uint32_t bg(uint32_t nowMs);
uint32_t fg(uint32_t nowMs);
void show(StatusBanner& banner, const char* detail, uint32_t nowMs);
void hide();
}  // namespace fired

}  // namespace ui
