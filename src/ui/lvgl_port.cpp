#include "lvgl_port.h"

#include <M5Unified.h>
#include <lvgl.h>

namespace lvgl_port {
namespace {

constexpr int32_t kBufferLines = 80;

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  const int32_t w = lv_area_get_width(area);
  const int32_t h = lv_area_get_height(area);
  // LVGL RGB565 is native-endian, which matches lgfx::rgb565_t.
  M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<const lgfx::rgb565_t*>(px));
  lv_display_flush_ready(disp);
}

void touchCb(lv_indev_t*, lv_indev_data_t* data) {
  if (M5.Touch.getCount() > 0) {
    const auto& t = M5.Touch.getDetail(0);
    data->point.x = t.x;
    data->point.y = t.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

uint32_t tickCb() { return millis(); }

}  // namespace

void begin() {
  lv_init();
  lv_tick_set_cb(tickCb);

  const int32_t w = M5.Display.width();
  const int32_t h = M5.Display.height();
  lv_display_t* disp = lv_display_create(w, h);

  const size_t bufBytes = size_t(w) * kBufferLines * 2;
  auto* buf = static_cast<uint8_t*>(heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (buf == nullptr) {
    buf = static_cast<uint8_t*>(heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM));
  }
  lv_display_set_buffers(disp, buf, nullptr, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchCb);
}

}  // namespace lvgl_port
