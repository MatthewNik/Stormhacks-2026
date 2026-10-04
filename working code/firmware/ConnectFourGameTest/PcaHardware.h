#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "Hatch.h"
#include <stdio.h>

class PcaHardware : public HatchIO {
  // PCA9685 power-on values: a match after configuration means the chip reset.
  static constexpr uint8_t POWER_ON_PRESCALE = 0x1E, SLEEP_BIT = 0x10;
  Adafruit_PWMServoDriver driver{Config::PCA_ADDRESS, Wire};
  uint8_t prescale = 0;
  unsigned retried = 0;
  char failure[192] = {};
  bool fail(const char *step, int code = -1, int observed = -1) {
    // Preserve the first failure; shutdown writes must not replace its cause.
    if (!failure[0]) snprintf(failure,sizeof(failure),"PCA detail: %s; address=0x%02X SDA=%d SCL=%d code=%d observed=%d retries=%u",
      step,unsigned(Config::PCA_ADDRESS),Config::SDA,Config::SCL,code,observed,retried);
    return false;
  }
  struct ReadError { const char *step; int code, observed; };
  bool readOnce(uint8_t address, uint8_t &value, ReadError &error) {
    // Match MotorCommandTest: one combined pointer-write/read transaction.
    Wire.beginTransmission(Config::PCA_ADDRESS);
    const bool buffered = Wire.write(address) == 1;
    const int combined = Wire.endTransmission(false);
    if (buffered && combined == 0 &&
        Wire.requestFrom(Config::PCA_ADDRESS, uint8_t(1)) == 1) {
      const int data = Wire.read();
      if (data >= 0) { value = uint8_t(data); return true; }
    }
    // Some controllers accept only STOP-separated reads. This never commands
    // a motor or substitutes a guessed register value.
    Wire.beginTransmission(Config::PCA_ADDRESS);
    if (Wire.write(address) != 1) { Wire.endTransmission(true); error = {"buffer failed",-1,-1}; return false; }
    const int status = Wire.endTransmission(true);
    if (status != 0) { error = {"select failed",status,-1}; return false; }
    const int bytes = Wire.requestFrom(Config::PCA_ADDRESS, size_t(1), true);
    if (bytes != 1) { error = {"read failed",-1,bytes}; return false; }
    const int data = Wire.read();
    if (data < 0) { error = {"byte missing",-1,data}; return false; }
    value = uint8_t(data); return true;
  }
  bool readRegister(uint8_t address, uint8_t &value) {
    ReadError error{"read failed",-1,-1};
    for (unsigned attempt = 0; attempt < Config::PCA_IO_ATTEMPTS; ++attempt) {
      if (attempt) { ++retried; delayMicroseconds(500); }
      if (readOnce(address,value,error)) return true;
    }
    char step[48];
    snprintf(step,sizeof(step),"register 0x%02X %s",unsigned(address),error.step);
    return fail(step,error.code,error.observed);
  }
  int setPwm(uint8_t channel, uint16_t on, uint16_t off) {
    int status = 0;
    for (unsigned attempt = 0; attempt < Config::PCA_IO_ATTEMPTS; ++attempt) {
      if (attempt) { ++retried; delayMicroseconds(500); }
      status = driver.setPWM(channel,on,off);
      if (status == 0) return 0;
    }
    return status;
  }
public:
  unsigned retries() const { return retried; }
  const char *error() const override { return failure[0] ? failure : "PCA detail: no failure recorded."; }
  void enable(bool enabled) override { digitalWrite(Config::OE, enabled ? LOW : HIGH); }
  bool stopChannel(uint8_t channel) override {
    if (channel >= 8) return fail("invalid stop channel",-1,channel);
    const int status = setPwm(channel,0,4096);
    return status == 0 || fail("channel FULL_OFF write failed",status,channel);
  }
  bool begin() {
    failure[0] = 0; retried = 0;
    enable(false);
    for (const auto &cal : Config::HATCHES) {
      if (cal.closed > 180 || cal.open > 180 || cal.minimumUs < 500 ||
          cal.maximumUs > 2500 || cal.minimumUs >= cal.maximumUs) return fail("invalid hatch calibration");
    }
    const auto &indexer = Config::INDEXER;
    if (indexer.closed > 180 || indexer.open > 180 || indexer.minimumUs < 500 ||
        indexer.maximumUs > 2500 || indexer.minimumUs >= indexer.maximumUs) return fail("invalid indexer calibration");
    if (!Wire.begin(Config::SDA, Config::SCL)) return fail("ESP32 I2C initialization failed");
    Wire.setTimeOut(50);
    Wire.beginTransmission(Config::PCA_ADDRESS);
    const int probe = Wire.endTransmission();
    if (probe != 0) return fail("address probe failed",probe);
    if (!driver.begin()) return fail("Adafruit driver initialization failed after address probe");
    driver.setOscillatorFrequency(Config::OSCILLATOR_HZ);
    driver.setPWMFreq(Config::PWM_HZ);
    if (!readRegister(0xFE, prescale)) return false;
    const float hz = float(Config::OSCILLATOR_HZ)/(4096.0f*(prescale+1));
    if (hz < 45 || hz > 55) return fail("PWM frequency outside 45..55 Hz",-1,prescale);
    if (!healthy()) return false;
    // FULL_OFF survives subsequent OE enabling; channels 8-15 are never commanded.
    for (uint8_t ch = 0; ch < 16; ++ch) {
      const int status = setPwm(ch,0,4096);
      if (status != 0) return fail("startup FULL_OFF write failed",status,ch);
    }
    return true;
  }
  bool healthy() {
    uint8_t current = 0, mode = 0;
    // A changed value is re-read so one corrupted byte cannot latch a fault.
    if (!readRegister(0xFE,current)) return false;
    if (current != prescale && !readRegister(0xFE,current)) return false;
    if (current != prescale) {
      if (current == POWER_ON_PRESCALE)
        return fail("PCA reset detected (prescaler at power-on value); logic supply dipped or chip browned out",prescale,current);
      return fail("PWM prescaler changed",prescale,current);
    }
    if (!readRegister(0x00,mode)) return false;
    if ((mode & SLEEP_BIT) && !readRegister(0x00,mode)) return false;
    if (mode & SLEEP_BIT) return fail("PCA remained in sleep mode (reset or brownout)",-1,mode);
    return true;
  }
  bool writeAngle(uint8_t channel, uint16_t angle) override {
    if (channel >= 8 || angle > 180) return fail("invalid motor command",channel,angle);
    if (channel < 7 && !Config::HATCH_ENABLED[channel]) return stopChannel(channel);
    const auto &cal = channel == 7 ? Config::INDEXER : Config::HATCHES[channel];
    const uint32_t us = cal.minimumUs + (uint32_t(cal.maximumUs-cal.minimumUs)*angle+90)/180;
    const uint64_t numerator = uint64_t(us)*Config::OSCILLATOR_HZ;
    const uint64_t denominator = uint64_t(prescale+1)*1000000;
    const uint16_t ticks = uint16_t((numerator+denominator/2)/denominator);
    const int status = setPwm(channel,0,ticks);
    return status == 0 || fail("motor position write failed",status,channel);
  }
};
