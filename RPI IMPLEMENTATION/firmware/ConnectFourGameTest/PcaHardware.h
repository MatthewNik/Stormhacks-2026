#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "Hatch.h"

class PcaHardware : public HatchIO {
  Adafruit_PWMServoDriver driver{Config::PCA_ADDRESS, Wire};
  uint8_t prescale = 0;
  bool readRegister(uint8_t address, uint8_t &value) {
    Wire.beginTransmission(Config::PCA_ADDRESS); Wire.write(address);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(Config::PCA_ADDRESS, uint8_t(1)) != 1) return false;
    value = uint8_t(Wire.read()); return true;
  }
public:
  void enable(bool enabled) override { digitalWrite(Config::OE, enabled ? LOW : HIGH); }
  bool stopChannel(uint8_t channel) override {
    return channel < 8 && driver.setPWM(channel,0,4096) == 0;
  }
  bool begin() {
    enable(false);
    for (const auto &cal : Config::HATCHES) {
      if (cal.closed > 180 || cal.open > 180 || cal.minimumUs < 500 ||
          cal.maximumUs > 2500 || cal.minimumUs >= cal.maximumUs) return false;
    }
    const auto &indexer = Config::INDEXER;
    if (indexer.closed > 180 || indexer.open > 180 || indexer.minimumUs < 500 ||
        indexer.maximumUs > 2500 || indexer.minimumUs >= indexer.maximumUs) return false;
    if (!Wire.begin(Config::SDA, Config::SCL)) return false;
    Wire.setTimeOut(50);
    if (!driver.begin()) return false;
    driver.setOscillatorFrequency(Config::OSCILLATOR_HZ);
    driver.setPWMFreq(Config::PWM_HZ);
    if (!readRegister(0xFE, prescale)) return false;
    const float hz = float(Config::OSCILLATOR_HZ)/(4096.0f*(prescale+1));
    if (hz < 45 || hz > 55 || !healthy()) return false;
    // FULL_OFF survives subsequent OE enabling; channels 8-15 are never commanded.
    for (uint8_t ch = 0; ch < 16; ++ch) if (driver.setPWM(ch,0,4096) != 0) return false;
    return true;
  }
  bool healthy() {
    uint8_t current = 0, mode = 0;
    return readRegister(0xFE,current) && current == prescale && readRegister(0x00,mode) && !(mode & 0x10);
  }
  bool writeAngle(uint8_t channel, uint16_t angle) override {
    if (channel >= 8 || angle > 180) return false;
    if (channel < 7 && !Config::HATCH_ENABLED[channel]) return stopChannel(channel);
    const auto &cal = channel == 7 ? Config::INDEXER : Config::HATCHES[channel];
    const uint32_t us = cal.minimumUs + (uint32_t(cal.maximumUs-cal.minimumUs)*angle+90)/180;
    const uint64_t numerator = uint64_t(us)*Config::OSCILLATOR_HZ;
    const uint64_t denominator = uint64_t(prescale+1)*1000000;
    const uint16_t ticks = uint16_t((numerator+denominator/2)/denominator);
    return driver.setPWM(channel,0,ticks) == 0;
  }
};
