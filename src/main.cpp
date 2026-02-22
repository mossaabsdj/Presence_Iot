
#include <Arduino.h>
#include "oled.h"
#include "finger.h"
#include "refid.h"
#include "server.h"

WiFiServerManager wifiServer;

// ===== pins =====
#define LED_PIN 2
#define button_PIN 13
String finalUid;   // هذا هو "UID النهائي" اللي راح تستعمله في النظام

#define RFID_SS_PIN   5
#define RFID_RST_PIN  27
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23

bool enrollMode = false;  // false = normal scan, true = enroll
bool buttonClicked() {
  static bool lastState = HIGH;
  bool currentState = digitalRead(button_PIN);

  if (lastState == HIGH && currentState == LOW) {
  //  delay(5);  // debounce
    lastState = currentState;
    return true;
  }
  lastState = currentState;
  return false;
}

void setup() {
  bool enrollMode = false;  // global
  // OLED
  oledBegin(0x3C, 22, 21);
  oledIntro();
  wifiServer.begin();

  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(button_PIN, INPUT_PULLUP);

 

  // Finger
 // مثال pins للبصمة (بدّلهم حسب wiring تاعك)
  // RX=16, TX=17 مثال شائع في ESP32
  fingerBegin(Serial2, 3, 1, 57600);

  // RFID
  rfidBegin(
    RFID_SS_PIN,
    RFID_RST_PIN,
    SPI_SCK,
    SPI_MISO,
    SPI_MOSI
  );

  //uint8_t v = rfidReadVersion();
 // Serial.print("RC522 Version: 0x");
// oledShowMessage(v, HEX);
}



void loop() {
  wifiServer.handle();

  // 1️⃣ التحقق من الزر وتبديل الوضع
  if (buttonClicked()) {
    enrollMode = !enrollMode;
    if (enrollMode)
      oledShowMessage("Enroll Mode ON", 2, true, 1000);
    else
      oledShowMessage("Normal Mode", 2, true, 800);
  }

  // 2️⃣ Enroll Mode
  if (enrollMode && fingerDetected()) {


    int id = fingerEnrollNew();
    if (id < 0) {
      oledShowMessage("Enroll Failed", 2, true, 1500);
      return;
    }

    oledShowMessage("New Finger\nID: " + String(id), 2, true, 800);

    oledShowMessage("Scan Card", 2, true, 0);
    String cardUid;
    while (!rfidReadUid(cardUid)) delay(20);

    fingerLinkUid(id, cardUid);
    finalUid = cardUid;

    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);

    oledShowMessage("Linked OK", 2, true, 1500);
    delay(500);

    enrollMode = false; // ارجع للوضع الطبيعي بعد التسجيل
    oledShowMessage("Normal Mode", 2, true, 500);
    return;
  }

  // 3️⃣ Normal Mode
  if (!enrollMode) {

    // 🔹 فحص البصمة
    if (fingerIsOk()) {
      if (fingerDetected()) {

        int id = fingerScanMatchId();
        if (id >= 0) {
          String linked;
          if (fingerGetLinkedUid(id, linked)) {
            finalUid = linked;
            oledShowMessage("Finger OK\n" + finalUid, 2, true, 800);
            return;
          } else {
            oledShowMessage("No UID\nScan Card", 2, true, 0);
            String cardUid;
            while (!rfidReadUid(cardUid)) delay(20);
            fingerLinkUid(id, cardUid);
            finalUid = cardUid;
            oledShowMessage("Linked OK", 2, true, 1500);
            return;
          }
        } else {
          oledShowMessage("No Match", 2, true, 800);
        }
      }
    }

   
  }

}
