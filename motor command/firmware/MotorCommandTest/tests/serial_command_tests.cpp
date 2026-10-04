#include <cstring>
#include <cassert>
#include <iostream>
#include "../MotorCommandTest.ino"

void send(const std::string &line) {
  Serial.input.insert(Serial.input.end(), line.begin(), line.end());
  while (Serial.available()) readSerialCommands();
}

int main() {
  setup();
  assert(controllerReady && outputEnable == HIGH);
  assert(servoDriver.writes.size() == 16);
  for (int channel = 0; channel < 16; ++channel) {
    assert(servoDriver.writes[channel] == std::make_tuple(channel, 0, 4096));
  }
  servoDriver.writes.clear();
  send("servo 0 0\nservo 0 90\rservo 7 180\r\nservo 1 50\n");
  assert(servoDriver.writes.size() == 4 && outputEnable == LOW);
  assert(servoDriver.writes[0] == std::make_tuple(0, 0, 102));
  assert(servoDriver.writes[1] == std::make_tuple(0, 0, 307));
  assert(servoDriver.writes[2] == std::make_tuple(7, 0, 512));
  assert(std::get<0>(servoDriver.writes[3]) == 1);
  const size_t previousWrites = servoDriver.writes.size();
  for (const char *invalid : {
      "servo -1 90", "servo 8 90", "servo 0 -1", "servo 0 181",
      "servo x 90", "servo 0 50.5", "servo 0 90 extra", "servo 0",
      "servo 0 99999999999999999999999999999999", "off 8", "help extra",
      "off all extra", "servo 0 90 extra more", "servo +1 90"}) {
    send(std::string(invalid) + "\n");
    assert(servoDriver.writes.size() == previousWrites);
  }
  send(std::string("servo 0 90\0extra\n", 17));
  send(std::string(120, 'x') + "\r\n");
  send("\n\r\n \t\nhelp\n");
  assert(servoDriver.writes.size() == previousWrites);
  send(" \tservo\t7 50 \r\nservo 7 50\n");
  assert(servoDriver.writes.size() == previousWrites + 2);
  send("off 7\n");
  assert(servoDriver.writes.back() == std::make_tuple(7, 0, 4096));
  send("off all\n");
  assert(outputEnable == HIGH);
  for (int channel = 0; channel < 16; ++channel) {
    assert(servoDriver.writes[servoDriver.writes.size() - 16 + channel] ==
           std::make_tuple(channel, 0, 4096));
  }
  send("servo 0 90\n");
  assert(outputEnable == LOW);
  servoDriver.failWrites = true;
  send("servo 1 50\n");
  assert(!controllerReady && outputEnable == HIGH);
  servoDriver.failWrites = false;
  const size_t faultWrites = servoDriver.writes.size();
  send("servo 0 90\noff all\n");
  assert(servoDriver.writes.size() == faultWrites);
  setup();
  assert(controllerReady && outputEnable == HIGH);
  send("servo 0 90\n");
  Wire.responds = false;
  clockMs += ServoConfig::BUS_CHECK_INTERVAL_MS;
  loop();
  assert(!controllerReady && outputEnable == HIGH);
  setup();
  assert(!controllerReady && outputEnable == HIGH);
  Wire.responds = true; setup();
  clockMs = 10000; send("servo 0 100\n");
  clockMs = 11500; send("ping\n"); loop(); assert(outputEnable == LOW);
  clockMs = 13499; loop(); assert(outputEnable == LOW);
  clockMs = 13500; loop(); assert(controllerReady && outputEnable == HIGH);
  for (bool active : activeChannels) assert(!active);
  send("servo 1 60\n"); assert(outputEnable == LOW);
  send("off 1\n"); assert(outputEnable == HIGH);
  clockMs = UINT32_MAX-500; send("servo 7 80\n");
  clockMs = 1498; loop(); assert(outputEnable == LOW);
  clockMs = 1499; loop(); assert(outputEnable == HIGH);
  send("info\nstatus\n");
  assert(Serial.output.find("MOTOR_TEST v1 READY") != std::string::npos);
  std::cout << "PASS: command parsing, pulse mapping, line recovery, startup, off, fault handling, heartbeat expiry and timer wraparound\n";
}
