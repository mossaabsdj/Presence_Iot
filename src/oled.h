#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

// Public functions
bool oledBegin(uint8_t address, uint8_t sdaPin, uint8_t sclPin) ;
void oledShowMessage(const String &msg, uint8_t textSize = 2, bool showLogo = true, uint16_t ms = 1500);
void oledAccessDenied(uint16_t ms = 1500);
void oledAccessValid(uint16_t ms = 1500);
void oledIntro();

#endif
