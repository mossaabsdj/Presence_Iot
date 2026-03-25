#include <Arduino.h>
#include "oled.h"
#include "finger.h"
#include "refid.h"
#include "server.h"
#include "data.h"
#include "DFPlayer.h"

HardwareSerial mySerial(2); // UART2
DFPlayer player(mySerial);

#define RXD2 33
#define TXD2 32
WiFiServerManager wifiServer;

// ===== pins =====
#define LED_PIN 2
#define button_PIN 13
String finalUid;   // هذا هو "UID النهائي" اللي راح تستعمله في النظام
int sallName;
#define RFID_SS_PIN   5
#define RFID_RST_PIN  27
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23
bool sessionActive = false;
unsigned long sessionStartTime = 0;
 unsigned long sessionDuration =5000; // 20 min in ms
String sessionUid = "";
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
  //=================MP3=========================================
    Serial.begin(115200);

    mySerial.begin(9600, SERIAL_8N1, RXD2, TXD2);

    player.begin(mySerial);

delay(2000);        // ← أضف هذا
player.volume(30);
delay(200);         // ← وهذا
player.play(1);   // delay(1000);

    Serial.println("DFPlayer Started");

//params sall=============================
  dataInit();   // MUST be called once
  sallName=getSall();
sessionDuration = getSessionDelay(); // e.g., returns 20*60*1000 for 20 minutes
//============================================
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





void handleEnrollAndFinger() {

  // Check button to toggle enroll mode
  if (buttonClicked()) {
    enrollMode = !enrollMode;
    if (enrollMode)
      oledShowMessage("Enroll Mode ON", 2, true, 1000);
    else
      oledShowMessage("Normal Mode", 2, true, 800);
  }

  // Enroll Mode
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

    enrollMode = false; // back to normal mode
    oledShowMessage("Normal Mode", 2, true, 500);
    return;
  }

  // Normal Mode Finger Check
  if (!enrollMode && fingerIsOk() && fingerDetected()) {
    int id = fingerScanMatchId();
    if (id >= 0) {
      String linked;
      if (fingerGetLinkedUid(id, linked)) {
        finalUid = linked;
        oledShowMessage("Finger OK\n" + finalUid, 2, true, 800);

        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
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
void loop() {
  wifiServer.handle();
//player.play(1);   // delay(1000);

  // --------------------------
  // 1️⃣ If session active → handle enroll/finger and show remaining time
  // --------------------------
  if (sessionActive) {
    unsigned long elapsed = millis() - sessionStartTime;

    if (elapsed >= sessionDuration) {
      sessionActive = false;
      oledShowMessage("Session Ended\nScan Card", 2, true, 0);
    } else {
      // Calculate remaining time
      unsigned long timeLeft = sessionDuration - elapsed;
      int minutes = timeLeft / 60000;
      int seconds = (timeLeft % 60000) / 1000;

      // Build OLED message dynamically
      String oledMsg = "";
      oledMsg += "Time left: ";
      if (minutes < 10) oledMsg += "0";
      oledMsg += String(minutes) + ":";
      if (seconds < 10) oledMsg += "0";
      oledMsg += String(seconds);

      oledShowMessage(oledMsg, 2, true, 500); // refresh every 500ms

      // Run enroll/finger logic
      handleEnrollAndFinger();
      return; // skip scanning new card
    }
  }

  // --------------------------
  // 2️⃣ Otherwise → prompt for card scan
  // --------------------------
  oledShowMessage("Scan Card", 2, true, 0);
  String cardUid;
  //there are problem here...??
  
  while (!rfidReadUid(cardUid)) {};
 // delay(2000);

  // Flash LED
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
oledShowMessage("Contacting Server...", 2, true, 0); // 0 = no timeout

  // Send UID to server
  String response = sendRequest("http://"+getServerIP()+":3000/api/CheckProf", cardUid);
oledShowMessage("", 2, true, 0); // clear OLED or show next message

  // --------------------------
  // 3️⃣ Handle server response
  // --------------------------
  if (response == "ERROR_404"&& response=="ERROR_UNKNOWN") {
        player.play(2);      // play 0003.mp3

    oledShowMessage("API Not Found!", 2, true, 1000);
    return;
  } 
  else if (response == "ERROR_500") {
        player.play(2);      // play 0003.mp3

    oledShowMessage("Server Error!", 2, true, 1000);
    return;
  } 
  else if (response == "ERROR_CONNECT") {
    delay(2000); 
player.volume(30);
delay(200);         // ← وهذا
player.play(2);   // delay(1000);    delay(2000); 

    oledShowMessage("Cannot Connect!", 2, true, 1000);
    return;
  } 
  else if (response != "" && response != "null") {
        player.play(3);      // play 0003.mp3

    // ✅ Access granted → start session
    sessionActive = true;
    sessionStartTime = millis();
    sessionUid = cardUid;

    // Show initial Access Granted message + time
    oledShowMessage("Access Granted " + response, 1, true, 2000);

    // Run enroll/finger logic immediately
    handleEnrollAndFinger();
    return;
  } 
  else {
    oledShowMessage("Access Denied", 2, true, 800);
  }
}