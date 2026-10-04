#include <Arduino.h>
#line 1 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\ColorOledSpiTest.ino"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1331.h>
#include "DisplayConfig.h"

Adafruit_SSD1331 display(&SPI, DisplayConfig::CHIP_SELECT_PIN,
                         DisplayConfig::DATA_COMMAND_PIN, DisplayConfig::RESET_PIN);

constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t RED = 0xF800;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t BLUE = 0x001F;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t YELLOW = 0xFFE0;
uint8_t currentPage = 0;
uint32_t pageStartedMs = 0;

#line 18 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\ColorOledSpiTest.ino"
void drawTextPage();
#line 38 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\ColorOledSpiTest.ino"
void drawColorBars();
#line 50 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\ColorOledSpiTest.ino"
void showTestPage(uint8_t page);
#line 63 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\ColorOledSpiTest.ino"
void setup();
#line 85 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\ColorOledSpiTest.ino"
void loop();
#line 18 "C:\\Users\\matth\\Documents\\Stormhacks2026 Oct 3-4\\Arduino\\ColorOledSpiTest\\ColorOledSpiTest.ino"
void drawTextPage() {
  display.fillScreen(BLACK);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.print("SSD1331 TEST");
  display.setTextColor(YELLOW);
  display.setCursor(5, 18);
  display.print("96x64 SPI");
  display.setTextColor(GREEN);
  display.setCursor(5, 31);
  display.print("Display test");
  display.setTextColor(WHITE);
  display.setCursor(5, 44);
  display.print(millis() / 1000);
  display.print(" sec");
  display.drawRect(0, 0, display.width(), display.height(), BLUE);
}

void drawColorBars() {
  display.fillScreen(BLACK);
  display.fillRect(0, 0, 32, 48, RED);
  display.fillRect(32, 0, 32, 48, GREEN);
  display.fillRect(64, 0, 32, 48, BLUE);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(12, 54); display.print("R");
  display.setCursor(44, 54); display.print("G");
  display.setCursor(76, 54); display.print("B");
}

void showTestPage(uint8_t page) {
  Serial.print("Drawing page ");
  Serial.print(page);
  Serial.print(": ");
  switch (page) {
    case 0: Serial.println("RED"); display.fillScreen(RED); break;
    case 1: Serial.println("GREEN"); display.fillScreen(GREEN); break;
    case 2: Serial.println("BLUE"); display.fillScreen(BLUE); break;
    case 3: Serial.println("TEXT / BORDER"); drawTextPage(); break;
    default: Serial.println("RGB BARS"); drawColorBars(); break;
  }
}

void setup() {
  // Prevent the existing PCA9685 from driving servos during display diagnostics.
  digitalWrite(DisplayConfig::PCA_OUTPUT_ENABLE_PIN, HIGH);
  pinMode(DisplayConfig::PCA_OUTPUT_ENABLE_PIN, OUTPUT);
  digitalWrite(DisplayConfig::CHIP_SELECT_PIN, HIGH);
  pinMode(DisplayConfig::CHIP_SELECT_PIN, OUTPUT);
  Serial.begin(DisplayConfig::SERIAL_BAUD);
  delay(100);
  Serial.println("\n96x64 SSD1331 SPI display test");
  Serial.println("SCL=D18, SDA=D19, RES=RX2/D16, DC=TX2/D17, CS=D26");
  Serial.println("VCC=3.3V, GND=common ground. PCA OE remains disabled on D25.");
  // No MISO: this display connection is write-only.
  SPI.begin(DisplayConfig::CLOCK_PIN, -1, DisplayConfig::DATA_PIN,
            DisplayConfig::CHIP_SELECT_PIN);
  display.begin(DisplayConfig::SPI_FREQUENCY_HZ);
  display.setRotation(0);
  display.enableDisplay(true);
  Serial.println("Initialization commands sent. Visually confirm the display; SPI has no acknowledgement.");
  showTestPage(currentPage);
  pageStartedMs = millis();
}

void loop() {
  const uint32_t nowMs = millis();
  if (uint32_t(nowMs - pageStartedMs) >= DisplayConfig::PAGE_INTERVAL_MS) {
    pageStartedMs = nowMs;
    currentPage = (currentPage + 1) % 5;
    showTestPage(currentPage);
  }
  delay(1);
}

