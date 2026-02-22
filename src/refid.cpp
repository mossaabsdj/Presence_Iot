#include "refid.h"

// RC522 object (global)
static MFRC522 rfid(0, 0);

void rfidBegin(
  uint8_t ssPin, uint8_t rstPin,
  uint8_t sck, uint8_t miso, uint8_t mosi
) {
  rfid = MFRC522(ssPin, rstPin);

  SPI.begin(sck, miso, mosi, ssPin);
  delay(50);

  rfid.PCD_Init();
  delay(50);
}

uint8_t rfidReadVersion() {
  return rfid.PCD_ReadRegister(MFRC522::VersionReg);
}

bool rfidReadUid(String &uidHex) {
  uidHex = "";

  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial())   return false;

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidHex += "0";
    uidHex += String(rfid.uid.uidByte[i], HEX);
  }
  uidHex.toUpperCase();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}
