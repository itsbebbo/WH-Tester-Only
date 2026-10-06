// GPIO assignments for the WH Board Tester (SPEC §3.2).
#pragma once

#include <cstdint>

namespace pins {

// Inputs from the WH Board (5 V logic via divider, active HIGH)
constexpr uint8_t POWER_IN = 5;
constexpr uint8_t ARM_IN = 6;
constexpr uint8_t FIRE_IN = 7;

// Outputs to the WH Board (via 3.3 V -> 5 V buffer, HIGH = ON)
constexpr uint8_t GREEN_OUT = 8;  // Port B
constexpr uint8_t RED_OUT = 9;    // Port B

// External load relays (HIGH = energised)
constexpr uint8_t LOAD_TEST = 17;    // Port C: LOW = open, HIGH = load connected
constexpr uint8_t LOAD_SELECT = 18;  // Port C: LOW = short, HIGH = 1 ohm

// INA219 on Port A (M5.Ex_I2C)
constexpr uint8_t I2C_SDA = 2;
constexpr uint8_t I2C_SCL = 1;

}  // namespace pins
