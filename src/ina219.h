// Minimal INA219 register driver on M5.Ex_I2C (SPEC §3.7).
#pragma once

#include <cstdint>

class Ina219 {
 public:
  // Writes the configuration and checks it reads back. Safe to call again
  // after the sensor has been unplugged/replugged.
  bool begin(uint8_t address);

  // Reads the shunt voltage register and converts to amps.
  bool readCurrent(float& amps);

 private:
  uint8_t addr_ = 0x40;
};
