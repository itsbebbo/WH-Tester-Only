#include "widgets.h"

#include <cstring>

#include "../beeper.h"
#include "config.h"

namespace ui {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
lv_obj_t* makeScreen() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(color::Black), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

lv_obj_t* makeBox(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t bg, int32_t radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t textColor, const char* text) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(textColor), 0);
  lv_label_set_text(l, text);
  return l;
}

void setText(lv_obj_t* label, const char* text) {
  if (std::strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void setBg(lv_obj_t* obj, uint32_t rgb) {
  if ((lv_color_to_u32(lv_obj_get_style_bg_color(obj, LV_PART_MAIN)) & 0xFFFFFF) != rgb) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), 0);
  }
}

void setTextColor(lv_obj_t* obj, uint32_t rgb) {
  if ((lv_color_to_u32(lv_obj_get_style_text_color(obj, LV_PART_MAIN)) & 0xFFFFFF) != rgb) {
    lv_obj_set_style_text_color(obj, lv_color_hex(rgb), 0);
  }
}

// ---------------------------------------------------------------------------
// Indicator
// ---------------------------------------------------------------------------
void Indicator::create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* title,
                       uint32_t onColor, bool stacked, const lv_font_t* titleFont, const lv_font_t* valueFont) {
  onColor_ = onColor;
  box_ = makeBox(parent, x, y, w, h, color::Off);
  title_ = makeLabel(box_, titleFont, color::OffText, title);
  value_ = makeLabel(box_, valueFont, color::OffText, "OFF");
  if (stacked) {
    lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_align(value_, LV_ALIGN_BOTTOM_MID, 0, -3);
  } else {
    lv_obj_align(title_, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_align(value_, LV_ALIGN_RIGHT_MID, -8, 0);
  }
}

void Indicator::set(bool on, const char* onText, const char* offText) {
  const uint32_t fg = on ? color::Black : color::OffText;
  setBg(box_, on ? onColor_ : color::Off);
  setTextColor(title_, fg);
  setTextColor(value_, fg);
  setText(value_, on ? onText : offText);
}

// ---------------------------------------------------------------------------
// ChipStrip
// ---------------------------------------------------------------------------
void ChipStrip::create(lv_obj_t* parent, int32_t y, int32_t h, const char* const names[], uint8_t count) {
  count_ = count > kMax ? kMax : count;
  const int32_t screenW = 320;
  for (uint8_t i = 0; i < count_; i++) {
    const int32_t x0 = (screenW * i) / count_;
    const int32_t x1 = (screenW * (i + 1)) / count_;
    chips_[i] = makeBox(parent, x0 + 1, y, x1 - x0 - 2, h, color::Off, 4);
    lv_obj_set_style_border_color(chips_[i], lv_color_hex(color::Yellow), 0);
    lv_obj_set_style_border_width(chips_[i], 0, 0);
    labels_[i] = makeLabel(chips_[i], &lv_font_montserrat_12, color::OffText, names[i]);
    lv_obj_center(labels_[i]);
  }
}

void ChipStrip::set(int active, uint32_t activeBg, uint32_t activeFg, int marked) {
  for (uint8_t i = 0; i < count_; i++) {
    const bool isActive = i == active;
    setBg(chips_[i], isActive ? activeBg : color::Off);
    setTextColor(labels_[i], isActive ? activeFg : color::OffText);
  }
  if (marked != lastMarked_) {
    for (uint8_t i = 0; i < count_; i++) {
      lv_obj_set_style_border_width(chips_[i], i == marked ? 2 : 0, 0);
    }
    lastMarked_ = marked;
  }
}

// ---------------------------------------------------------------------------
// StatusBanner
// ---------------------------------------------------------------------------
void StatusBanner::create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* tag) {
  box_ = makeBox(parent, x, y, w, h, color::NoPower, 6);

  tag_ = makeLabel(box_, &lv_font_montserrat_12, color::White, tag);
  lv_obj_align(tag_, LV_ALIGN_TOP_LEFT, 4, 2);

  main_ = makeLabel(box_, &lv_font_montserrat_28, color::White, "");
  lv_obj_align(main_, LV_ALIGN_TOP_MID, 0, 0);

  detail_ = makeLabel(box_, &lv_font_montserrat_14, color::White, "");
  lv_label_set_long_mode(detail_, LV_LABEL_LONG_DOT);
  lv_obj_set_width(detail_, w - 8);
  lv_obj_set_style_text_align(detail_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_radius(detail_, 3, 0);
  lv_obj_set_style_bg_color(detail_, lv_color_hex(color::Black), 0);
  lv_obj_set_style_bg_opa(detail_, LV_OPA_TRANSP, 0);
  lv_obj_align(detail_, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void StatusBanner::set(const char* mainText, const char* detailText, uint32_t bg, uint32_t fg, bool detailAlert) {
  setBg(box_, bg);
  setTextColor(tag_, fg);
  setTextColor(main_, fg);
  setText(main_, mainText);
  setText(detail_, detailText);
  if (int8_t(detailAlert) != lastAlert_) {
    lv_obj_set_style_bg_opa(detail_, detailAlert ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lastAlert_ = detailAlert;
  }
  setTextColor(detail_, detailAlert ? color::Yellow : fg);
}

// ---------------------------------------------------------------------------
// TapButton
// ---------------------------------------------------------------------------
void TapButton::create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text,
                       const lv_font_t* font, Callback cb, intptr_t arg) {
  cb_ = cb;
  arg_ = arg;
  btn_ = makeBox(parent, x, y, w, h, color::ButtonBg, 8);
  lv_obj_add_flag(btn_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_color(btn_, lv_color_hex(color::ButtonBorder), 0);
  lv_obj_set_style_border_width(btn_, 2, 0);
  lv_obj_set_style_border_color(btn_, lv_color_hex(color::White), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(btn_, 4, LV_STATE_PRESSED);

  label_ = makeLabel(btn_, font, color::White, text);
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label_);

  lv_obj_add_event_cb(btn_, onEvent, LV_EVENT_ALL, this);
}

void TapButton::setColors(uint32_t bg, uint32_t fg) {
  setBg(btn_, bg);
  setTextColor(label_, fg);
}

void TapButton::onEvent(lv_event_t* e) {
  auto* self = static_cast<TapButton*>(lv_event_get_user_data(e));
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      beeper::click();
      break;
    case LV_EVENT_CLICKED:
      if (self->cb_) self->cb_(self->arg_);
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// HoldButton
// ---------------------------------------------------------------------------
void HoldButton::create(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text,
                        uint32_t accent, Callback cb, uint32_t holdMs) {
  cb_ = cb;
  width_ = w;
  holdMs_ = holdMs;
  btn_ = makeBox(parent, x, y, w, h, color::ButtonBg, 8);
  lv_obj_add_flag(btn_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_color(btn_, lv_color_hex(color::ButtonBorder), 0);
  lv_obj_set_style_border_width(btn_, 2, 0);
  lv_obj_set_style_border_color(btn_, lv_color_hex(color::White), LV_STATE_PRESSED);
  lv_obj_set_style_clip_corner(btn_, true, 0);

  fill_ = makeBox(btn_, 0, 0, 0, h, accent, 0);

  label_ = makeLabel(btn_, &lv_font_montserrat_24, color::White, text);
  lv_obj_align(label_, LV_ALIGN_CENTER, 0, -6);

  hint_ = makeLabel(btn_, &lv_font_montserrat_12, color::OffText, "HOLD 1 s");
  lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -3);
  if (holdMs_ == 0) {
    lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(label_, LV_ALIGN_CENTER, 0, 0);
  }

  lv_obj_add_event_cb(btn_, onEvent, LV_EVENT_ALL, this);
}

void HoldButton::setEnabled(bool enabled) {
  if (enabled == enabled_) return;
  enabled_ = enabled;
  setTextColor(label_, enabled ? color::White : 0x505050);
  setTextColor(hint_, enabled ? color::OffText : 0x404040);
  lv_obj_set_style_border_color(btn_, lv_color_hex(enabled ? color::ButtonBorder : 0x404040), 0);
  if (!enabled) {
    pressing_ = false;
    lastFillW_ = -1;
    lv_obj_set_width(fill_, 0);
  }
}

void HoldButton::onEvent(lv_event_t* e) {
  auto* self = static_cast<HoldButton*>(lv_event_get_user_data(e));
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      if (!self->enabled_) break;
      self->pressing_ = true;
      self->triggered_ = false;
      self->pressStartMs_ = lv_tick_get();
      beeper::click();
      break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
      self->pressing_ = false;
      self->lastFillW_ = 0;
      lv_obj_set_width(self->fill_, 0);
      break;
    default:
      break;
  }
}

void HoldButton::update(uint32_t nowMs) {
  if (!pressing_ || !enabled_) return;
  const uint32_t held = nowMs - pressStartMs_;
  int32_t w = held >= holdMs_ ? width_ : int32_t((int64_t(width_) * held) / holdMs_);
  if (w != lastFillW_) {
    lv_obj_set_width(fill_, w);
    lastFillW_ = w;
  }
  if (held >= holdMs_ && !triggered_) {
    triggered_ = true;
    if (cb_) cb_();
  }
}

// ---------------------------------------------------------------------------
// Shared FIRED indication
// ---------------------------------------------------------------------------
namespace fired {
namespace {
lv_obj_t* border = nullptr;
}

void begin() {
  border = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(border);
  lv_obj_set_size(border, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_opa(border, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(border, 6, 0);
  lv_obj_set_style_border_color(border, lv_color_hex(color::Red), 0);
  lv_obj_set_style_border_opa(border, LV_OPA_COVER, 0);
  lv_obj_remove_flag(border, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(border, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(border, LV_OBJ_FLAG_HIDDEN);
}

bool phase(uint32_t nowMs) { return (nowMs / cfg::FIRED_FLASH_MS) % 2 == 0; }
uint32_t bg(uint32_t nowMs) { return phase(nowMs) ? color::Red : color::Yellow; }
uint32_t fg(uint32_t nowMs) { return phase(nowMs) ? color::Yellow : color::Red; }

void show(StatusBanner& banner, const char* detail, uint32_t nowMs) {
  banner.set(LV_SYMBOL_WARNING " FIRED " LV_SYMBOL_WARNING, detail, bg(nowMs), fg(nowMs));
  if ((lv_color_to_u32(lv_obj_get_style_border_color(border, LV_PART_MAIN)) & 0xFFFFFF) != bg(nowMs)) {
    lv_obj_set_style_border_color(border, lv_color_hex(bg(nowMs)), 0);
  }
  if (lv_obj_has_flag(border, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(border, LV_OBJ_FLAG_HIDDEN);
}

void hide() {
  if (border && !lv_obj_has_flag(border, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(border, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace fired

}  // namespace ui
