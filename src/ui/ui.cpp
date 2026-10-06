#include "ui.h"

#include <lvgl.h>

#include "../io_task.h"
#include "config.h"
#include "lvgl_port.h"
#include "screens.h"
#include "widgets.h"

namespace ui {
namespace {

Screen current = Screen::Menu;
lv_obj_t* screens[3] = {};
uint32_t lastRefreshMs = 0;

}  // namespace

void begin() {
  lvgl_port::begin();
  fired::begin();
  screens[int(Screen::Menu)] = menu_screen::create();
  screens[int(Screen::Internal)] = internal_screen::create();
  screens[int(Screen::External)] = external_screen::create();
  lv_screen_load(screens[int(Screen::Menu)]);
}

void show(Screen s) {
  current = s;
  fired::hide();
  lv_screen_load(screens[int(s)]);

  io::Command c{io::CmdType::EnterMenu, LoadMode::Open};
  if (s == Screen::Internal) c.type = io::CmdType::EnterInternal;
  if (s == Screen::External) c.type = io::CmdType::EnterExternal;
  io::send(c);
}

void update(uint32_t nowMs) {
  switch (current) {
    case Screen::Menu: menu_screen::tick(nowMs); break;
    case Screen::Internal: internal_screen::tick(nowMs); break;
    case Screen::External: external_screen::tick(nowMs); break;
  }

  if (nowMs - lastRefreshMs >= cfg::UI_REFRESH_MS) {
    lastRefreshMs = nowMs;
    io::Snapshot snap;
    io::getSnapshot(snap);
    // Skip until the I/O task has acted on the last screen change, so a
    // stale state is never drawn on the new screen.
    if (snap.screen == current) {
      switch (current) {
        case Screen::Menu: menu_screen::update(snap, nowMs); break;
        case Screen::Internal: internal_screen::update(snap, nowMs); break;
        case Screen::External: external_screen::update(snap, nowMs); break;
      }
    }
  }

  lv_timer_handler();
}

}  // namespace ui
