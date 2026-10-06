#include "ina219.h"

#include <M5Unified.h>

#include "config.h"

namespace {

constexpr uint8_t REG_CONFIG = 0x00;
constexpr uint8_t REG_SHUNT_VOLTAGE = 0x01;

// BRNG=32V, PGA=/8 (+-320 mV), BADC=12-bit, SADC=12-bit 1 sample (532 us),
// MODE=shunt voltage continuous.
constexpr uint16_t CONFIG_VALUE = 0x399D;

constexpr float SHUNT_LSB_V = 10e-6f;  // 10 uV per count

}  // namespace

bool Ina219::begin(uint8_t address) {
  addr_ = address;
  const uint8_t cfgBytes[2] = {uint8_t(CONFIG_VALUE >> 8), uint8_t(CONFIG_VALUE & 0xFF)};
  if (!M5.Ex_I2C.writeRegister(addr_, REG_CONFIG, cfgBytes, 2, cfg::INA219_I2C_HZ)) {
    return false;
  }
  uint8_t rb[2] = {0, 0};
  if (!M5.Ex_I2C.readRegister(addr_, REG_CONFIG, rb, 2, cfg::INA219_I2C_HZ)) {
    return false;
  }
  return ((uint16_t(rb[0]) << 8) | rb[1]) == CONFIG_VALUE;
}

bool Ina219::readCurrent(float& amps) {
  uint8_t buf[2];
  if (!M5.Ex_I2C.readRegister(addr_, REG_SHUNT_VOLTAGE, buf, 2, cfg::INA219_I2C_HZ)) {
    return false;
  }
  const int16_t raw = int16_t((uint16_t(buf[0]) << 8) | buf[1]);
  amps = (float(raw) * SHUNT_LSB_V) / cfg::R_SHUNT_OHMS;
  return true;
}
