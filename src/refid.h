#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

// ===== init =====
void rfidBegin(
  uint8_t ssPin, uint8_t rstPin,
  uint8_t sck, uint8_t miso, uint8_t mosi
);

// ===== read =====
bool rfidReadUid(String &uidHex);

// ===== debug =====
uint8_t rfidReadVersion();
