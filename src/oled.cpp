#include "oled.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SDA_PIN 22
#define SCL_PIN 21

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Logo
#define LOGO_WIDTH 16
#define LOGO_HEIGHT 16

static const unsigned char PROGMEM logo_bmp[] = {
  0x00,0x00,0x7E,0x00,0x81,0x00,0xA5,0x00,0x81,0x00,0xBD,0x00,0x81,0x00,0x7E,0x00,
  0x00,0x00,0x00,0x00,0x3C,0x00,0x42,0x00,0xA5,0x00,0xA1,0x00,0x42,0x00,0x3C,0x00
};

static void oledDrawLogoCenter(int y = 0) {
  int x = (display.width() - LOGO_WIDTH) / 2;
  display.drawBitmap(x, y, logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
}


bool oledBegin(uint8_t address, uint8_t sdaPin, uint8_t sclPin) {
  Wire.begin(sdaPin, sclPin);

  if (!display.begin(SSD1306_SWITCHCAPVCC, address)) {
    return false;
  }

  display.clearDisplay();
  display.display();
  return true;
}

void oledShowMessage(const String &msg, uint8_t textSize, bool showLogo, uint16_t ms) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  int y = 0;
  if (showLogo) {
    oledDrawLogoCenter(0);
    y = LOGO_HEIGHT + 6;
  }

  display.setTextSize(textSize);
  display.setCursor(0, y);
  display.println(msg);
  display.display();
delay(ms);
}

void oledAccessDenied(uint16_t ms) {
  display.clearDisplay();
  oledDrawLogoCenter(0);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, LOGO_HEIGHT + 6);
  display.println("ACCESS");
  display.setCursor(10, LOGO_HEIGHT + 28);
  display.println("DENIED");

  display.display();
  delay(ms);
}

void oledAccessValid(uint16_t ms) {
  display.clearDisplay();
  oledDrawLogoCenter(0);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, LOGO_HEIGHT + 6);
  display.println("ACCESS");
  display.setCursor(10, LOGO_HEIGHT + 28);
  display.println("VALID");

  display.display();
  delay(ms);
}

void oledIntro() {
  oledShowMessage("Welcome!", 2, true, 1200);
  oledShowMessage("System\nReady", 2, false, 1200);
}
