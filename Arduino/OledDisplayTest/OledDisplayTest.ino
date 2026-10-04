#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "DisplayConfig.h"

Adafruit_SH1106G display(DisplayConfig::WIDTH, DisplayConfig::HEIGHT, &Wire,
                        DisplayConfig::RESET_PIN, DisplayConfig::I2C_FREQUENCY_HZ,
                        DisplayConfig::I2C_FREQUENCY_HZ);
bool displayReady = false;
uint8_t displayAddress = 0;
uint32_t lastFrameMs = 0;
uint32_t pageStartedMs = 0;
uint8_t currentPage = 0;

bool deviceResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void printHexAddress(uint8_t address) {
  Serial.print("0x");
  if (address < 16) Serial.print('0');
  Serial.print(address, HEX);
}

void scanI2cBus() {
  Serial.println("I2C scan: SDA D21, SCL D22");
  uint8_t foundCount = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    if (!deviceResponds(address)) continue;
    Serial.print("Found ");
    printHexAddress(address);
    Serial.println();
    ++foundCount;
  }
  if (foundCount == 0) Serial.println("No devices found. Check power, ground, SDA and SCL.");
}

bool selectDisplayAddress() {
  if (DisplayConfig::OLED_ADDRESS != 0) {
    displayAddress = DisplayConfig::OLED_ADDRESS;
    return deviceResponds(displayAddress);
  }
  const bool responds3c = deviceResponds(0x3C);
  const bool responds3d = deviceResponds(0x3D);
  if (responds3c && responds3d) {
    Serial.println("Both 0x3C and 0x3D respond. Set OLED_ADDRESS explicitly in DisplayConfig.h.");
    return false;
  }
  if (!responds3c && !responds3d) return false;
  displayAddress = responds3c ? 0x3C : 0x3D;
  return true;
}

void drawStatusPage(uint32_t nowMs) {
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.print("SH1106 OLED TEST");
  display.setCursor(5, 18);
  display.print("Address: 0x");
  display.print(displayAddress, HEX);
  display.setTextSize(2);
  display.setCursor(5, 32);
  display.print(nowMs / 1000);
  display.print(" sec");
  const uint16_t barWidth = (nowMs % DisplayConfig::PAGE_INTERVAL_MS) * 116UL /
                            DisplayConfig::PAGE_INTERVAL_MS;
  display.drawRect(5, 53, 118, 6, SH110X_WHITE);
  display.fillRect(6, 54, barWidth, 4, SH110X_WHITE);
}

void drawShapePage() {
  display.setTextSize(1);
  display.setCursor(5, 5);
  display.print("SHAPES / EDGE TEST");
  display.drawRect(7, 22, 28, 32, SH110X_WHITE);
  display.drawLine(7, 22, 34, 53, SH110X_WHITE);
  display.drawCircle(62, 38, 15, SH110X_WHITE);
  display.fillCircle(101, 38, 15, SH110X_WHITE);
}

void drawCheckerboardPage() {
  for (int16_t y = 0; y < DisplayConfig::HEIGHT; y += 8) {
    for (int16_t x = 0; x < DisplayConfig::WIDTH; x += 8) {
      if (((x / 8) + (y / 8)) % 2 == 0) {
        display.fillRect(x, y, 8, 8, SH110X_WHITE);
      }
    }
  }
}

void renderTestPage(uint32_t nowMs) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);
  if (currentPage == 0) drawStatusPage(nowMs);
  else if (currentPage == 1) drawShapePage();
  else drawCheckerboardPage();
  if (currentPage != 2) {
    display.drawRect(0, 0, DisplayConfig::WIDTH, DisplayConfig::HEIGHT, SH110X_WHITE);
  }
  display.display();
}

void setup() {
  // Disable the shared-bus PCA outputs; this test never commands a servo.
  digitalWrite(DisplayConfig::PCA_OUTPUT_ENABLE_PIN, HIGH);
  pinMode(DisplayConfig::PCA_OUTPUT_ENABLE_PIN, OUTPUT);
  Serial.begin(DisplayConfig::SERIAL_BAUD);
  delay(100);
  Serial.println("\nSH1106 128x64 OLED test. PCA outputs disabled via D25.");
  if (!Wire.begin(DisplayConfig::SDA_PIN, DisplayConfig::SCL_PIN,
                  DisplayConfig::I2C_FREQUENCY_HZ)) {
    Serial.println("ERROR: I2C initialization failed. Reset after checking wiring.");
    return;
  }
  Wire.setTimeOut(50);
  scanI2cBus();
  if (!selectDisplayAddress()) {
    Serial.println("ERROR: OLED address not selected. Check wiring/address and reset.");
    return;
  }
  if (!display.begin(displayAddress, false)) {
    Serial.println("ERROR: SH1106 initialization failed. Check wiring and available memory.");
    return;
  }
  displayReady = true;
  pageStartedMs = lastFrameMs = millis();
  renderTestPage(lastFrameMs);
  Serial.print("READY: OLED at ");
  printHexAddress(displayAddress);
  Serial.println(". Cycling text, shapes and checkerboard every 3 seconds.");
}

void loop() {
  if (displayReady) {
    const uint32_t nowMs = millis();
    if (uint32_t(nowMs - pageStartedMs) >= DisplayConfig::PAGE_INTERVAL_MS) {
      currentPage = (currentPage + 1) % 3;
      pageStartedMs = nowMs;
    }
    if (uint32_t(nowMs - lastFrameMs) >= DisplayConfig::FRAME_INTERVAL_MS) {
      lastFrameMs = nowMs;
      if (!deviceResponds(displayAddress)) {
        displayReady = false;
        Serial.println("ERROR: OLED stopped responding. Check wiring with power off, then reset.");
      } else {
        renderTestPage(nowMs);
      }
    }
  }
  delay(1);
}
