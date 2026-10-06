// All timing and threshold constants (SPEC §12).
// Kept free of Arduino headers so the native unit tests can include it.
#pragma once

#include <cstdint>

#include "tester_types.h"

#define FW_VERSION "0.2.4"

namespace cfg {

// ---- Internal mode --------------------------------------------------------
constexpr uint32_t INPUT_DEBOUNCE_MS = 5;
constexpr uint32_t SETTLING_TIME_MS = 5000;
constexpr uint32_t ARM_TO_FIRE_MIN_MS = 1000;
constexpr uint32_t POWER_LOSS_GRACE_MS = 50;

// Toggle intervals (half-period: ON for this long, then OFF for this long)
constexpr uint32_t BLINK_SETTLING_MS = 600;
constexpr uint32_t BLINK_READY_MS = 166;
constexpr uint32_t BLINK_ERROR_MS = 100;

// ---- External mode --------------------------------------------------------
constexpr float FIRE_CURRENT_A = 1.200f;
constexpr uint32_t FIRE_QUALIFY_MS = 5;
constexpr uint16_t FIRE_MIN_SAMPLES = 4;
constexpr uint32_t EXT_FIRED_HOLD_MS = 5000;  // FIRED stays shown this long after current is lost

constexpr float R_SHUNT_OHMS = 0.100f;
constexpr uint8_t INA219_ADDR = 0x40;
constexpr uint32_t INA219_I2C_HZ = 400000;
constexpr uint8_t SENSOR_FAIL_COUNT = 3;
constexpr uint8_t SENSOR_RECOVER_COUNT = 10;
constexpr uint32_t SENSOR_RETRY_MS = 100;  // read retry period while faulted
constexpr uint16_t CURRENT_AVG_SAMPLES = 50;

constexpr LoadMode EXTERNAL_DEFAULT_LOAD = LoadMode::OneOhm;  // selected on entering External

constexpr uint32_t RELAY_RELEASE_MS = 20;
constexpr uint32_t RELAY_SETTLE_MS = 20;
constexpr uint32_t DETECT_BLANKING_MS = 50;

// ---- User interface -------------------------------------------------------
constexpr uint32_t FIRED_FLASH_MS = 250;
constexpr uint32_t ERROR_FLASH_MS = 250;
constexpr uint32_t HOLD_BUTTON_MS = 1000;
constexpr uint32_t UI_REFRESH_MS = 30;
constexpr float CURRENT_BAR_MAX_A = 2.0f;
constexpr uint8_t BEEP_VOLUME = 200;
constexpr uint8_t DISPLAY_BRIGHTNESS = 255;

// ---- Tasks ----------------------------------------------------------------
constexpr uint32_t IO_TASK_WDT_MS = 2000;

}  // namespace cfg
