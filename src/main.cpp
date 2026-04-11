#include <Arduino.h>
#include "oled.h"
#include "finger.h"
#include "refid.h"
#include "server.h"
#include "data.h"
#include "DFPlayer.h"
#include <ArduinoJson.h>
HardwareSerial mySerial(2);
HardwareSerial fingerSerial(1);
DFPlayer player(mySerial);

#define RXD2 33
#define TXD2 32
WiFiServerManager wifiServer;

// ===== pins =====
#define BUZZER_PIN    2
#define LED_GREEN_PIN 14
#define LED_RED_PIN   4
#define button_PIN    13
String finalUid;
int sallName;
#define RFID_SS_PIN   5
#define RFID_RST_PIN  27
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  23
bool sessionActive = false;
unsigned long sessionStartTime = 0;
unsigned long sessionDuration = 5000;
String sessionUid = "";
String token = "";

bool enrollMode = false;
String response = "";

// ===== Green LED blink state =====
unsigned long lastGreenToggle = 0;
bool greenState = false;
bool redActive = false;

// ===== Forward declaration =====

// ===== Non-blocking button state machine =====
enum ButtonResult { BTN_NONE = 0, BTN_CLICK = 1, BTN_HOLD = 2 };
void handleEnrollAndFinger(String session_id, ButtonResult btn);

ButtonResult readButton() {
  static bool wasPressed       = false;
  static bool holdFired        = false;
  static unsigned long pressStart = 0;

  bool pressed = (digitalRead(button_PIN) == LOW);

  if (pressed && !wasPressed) {
    pressStart = millis();
    holdFired  = false;
    wasPressed = true;
  }

  if (pressed && wasPressed && !holdFired) {
    if (millis() - pressStart >= 5000) {
      holdFired = true;
      return BTN_HOLD;
    }
  }

  if (!pressed && wasPressed) {
    wasPressed = false;
    if (!holdFired && (millis() - pressStart) >= 50) {
      return BTN_CLICK;
    }
  }

  return BTN_NONE;
}

// ===== LED helpers =====
void blinkGreenLed() {
  if (redActive) return;
  unsigned long now = millis();
  if (now - lastGreenToggle >= 500) {
    lastGreenToggle = now;
    greenState = !greenState;
    digitalWrite(LED_GREEN_PIN, greenState ? HIGH : LOW);
  }
}

void showRedAlert(int durationMs) {
  redActive = true;
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, HIGH);
  delay(durationMs);
  digitalWrite(LED_RED_PIN, LOW);
  redActive = false;
  lastGreenToggle = millis();
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("DFPlayer démarré");

  token = getToken();
  dataInit();
  sallName = getSall();
  sessionDuration = getSessionDelay();

  enrollMode = false;

  oledBegin(0x3C, 22, 21);
  oledIntro();
  wifiServer.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(button_PIN, INPUT_PULLUP);

  fingerBegin(fingerSerial, 16, 17, 57600);
  fingerSleep();

  rfidBegin(RFID_SS_PIN, RFID_RST_PIN, SPI_SCK, SPI_MISO, SPI_MOSI);

  player.begin(mySerial);
  delay(2000);
  player.volume(30);
  delay(200);
  player.play(1);
}

// ===== Sound helper =====
void PlaySound(uint8_t value) {
  player.volume(30);
  delay(200);
  player.play(value);
}

// ===== Enroll + fingerprint handler =====
// btn is read ONCE in loop() and passed here — never calls readButton() internally
void handleEnrollAndFinger(String session_id, ButtonResult btn) {

  // Short click → toggle enroll mode
  if (btn == BTN_CLICK) {
    enrollMode = !enrollMode;
    if (enrollMode) {
      oledShowMessage("Mode Inscription ON", 2, true, 500);
    } else {
      oledShowMessage("Mode Normal", 2, true, 500);
      PlaySound(6);
    }
    return;
  }

  // ---- Enroll mode ----
  if (enrollMode) {
    if (!fingerDetected()) return;

    int id = fingerEnrollNew();
    if (id < 0) {
      oledShowMessage("Échec inscription", 2, true, 1500);
      return;
    }

    oledShowMessage("Nouveau doigt\nID: " + String(id), 2, true, 800);
    oledShowMessage("Scanner la carte", 2, true, 0);

    String cardUid;
    while (!rfidReadUid(cardUid)) delay(20);

    fingerLinkUid(id, cardUid);
    finalUid = cardUid;

    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);

    oledShowMessage("Liaison réussie", 2, true, 1500);
    delay(500);
    enrollMode = false;
    oledShowMessage("Mode Normal", 2, true, 500);
    return;
  }

  // ---- Normal mode — fingerprint verify ----
  if (!enrollMode && fingerIsOk() && fingerDetected()) {
    int id = fingerScanMatchId();
    if (id >= 0) {
      String linked;
      if (fingerGetLinkedUid(id, linked)) {
        finalUid = linked;
        oledShowMessage("Doigt OK\n" + finalUid, 2, true, 800);

        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);

        String msg = sendRequestEtudiant(
          "http://" + getServerIP() + ":3000/api/CheckEtudiant",
          finalUid, session_id, token
        );

        if (msg == "false") {
          PlaySound(5);
          showRedAlert(1500);
          PlaySound(12);
          oledShowMessage("Étudiant non reconnu", 2, true, 800);
        } else if (msg == "ERROR" || msg == "ERROR_WIFI") {
          PlaySound(4);
          oledShowMessage("Erreur serveur", 2, true, 800);
        } else if (msg != "" && msg != "null") {
          PlaySound(11);
          oledShowMessage("Présence enregistrée", 2, true, 800);
        }

      } else {
        oledShowMessage("Pas de carte liée\nScanner la carte", 2, true, 0);
        String cardUid;
        while (!rfidReadUid(cardUid)) delay(20);
        fingerLinkUid(id, cardUid);
        finalUid = cardUid;
        oledShowMessage("Liaison réussie", 2, true, 1500);
      }
    } else {
      PlaySound(5);
      showRedAlert(1000);
      oledShowMessage("Aucune correspondance", 2, true, 800);
    }
  }
}

// ===== Main loop =====
void loop() {
  wifiServer.handle();
  blinkGreenLed();

  // Read button ONCE per iteration — result passed to whoever needs it
  ButtonResult btn = readButton();

  // --------------------------
  // 1️⃣ Session active
  // --------------------------
  if (sessionActive) {
    unsigned long elapsed = millis() - sessionStartTime;

    if (elapsed >= sessionDuration) {
      sessionActive = false;
      enrollMode    = false;
      fingerSleep();
      PlaySound(10);
      oledShowMessage("Session terminée\nScanner la carte", 2, true, 0);
      return;
    }

    // Long hold → professor wants to close session
    if (btn == BTN_HOLD) {
      oledShowMessage("Scanner la carte\npour quitter", 2, true, 0);
      String quitUid;
      unsigned long scanTimeout = millis();
      bool gotCard = false;
      while (millis() - scanTimeout < 10000) {
        if (rfidReadUid(quitUid)) { gotCard = true; break; }
        delay(20);
      }
      if (!gotCard) {
        oledShowMessage("Délai dépassé.\nAnnulé.", 2, true, 1000);
      } else if (quitUid == sessionUid) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        sessionActive = false;
        enrollMode    = false;
        fingerSleep();
        PlaySound(10);
        oledShowMessage("Session fermée\npar le professeur", 2, true, 1500);
        return;
      } else {
        PlaySound(5);
        showRedAlert(1000);
        oledShowMessage("Carte incorrecte!\nAnnulé.", 2, true, 1000);
      }
      return;
    }

    // Show countdown
    unsigned long timeLeft = sessionDuration - elapsed;
    int minutes = timeLeft / 60000;
    int seconds = (timeLeft % 60000) / 1000;
    String oledMsg = "Temps restant: ";
    if (minutes < 10) oledMsg += "0";
    oledMsg += String(minutes) + ":";
    if (seconds < 10) oledMsg += "0";
    oledMsg += String(seconds);
    oledShowMessage(oledMsg, 2, true, 0);

    // Pass the already-read btn — no second readButton() call
    handleEnrollAndFinger(response, btn);
    return;
  }

  // --------------------------
  // 2️⃣ Attente scan carte
  // --------------------------
  oledShowMessage("Scanner la carte", 2, true, 0);
  String cardUid;
  if (!rfidReadUid(cardUid)) return;

  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);

  oledShowMessage("Connexion serveur...", 2, true, 0);
 response = sendRequest(
    "http://" + getServerIP() + ":3000/api/CheckProf",
    cardUid, sallName, sessionDuration, token
  );

  // --------------------------
  // 3️⃣ Traitement réponse serveur
  // --------------------------

  // Network / HTTP errors (not from API, from sendRequest itself)
  if (response == "ERROR_WIFI") {
    PlaySound(4); // TTS: "Pas de connexion WiFi"
    oledShowMessage("Pas de WiFi!", 2, true, 1000);
    return;
  }
  if (response == "ERROR_CONNECT") {
    PlaySound(4); // TTS: "Connexion au serveur impossible"
    oledShowMessage("Connexion impossible!", 2, true, 1000);
    return;
  }
  if (response == "ERROR_404") {
    PlaySound(4); // TTS: "API introuvable"
    oledShowMessage("API introuvable!", 2, true, 1000);
    return;
  }
  if (response == "ERROR") {
    PlaySound(4); // TTS: "Erreur inconnue"
    oledShowMessage("Erreur inconnue!", 2, true, 1000);
    return;
  }

  // Parse JSON from API
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, response);

  if (err) {
    PlaySound(4); // TTS: "Réponse serveur invalide"
    oledShowMessage("Réponse invalide!", 2, true, 1000);
    Serial.println("JSON parse error: " + String(err.c_str()));
    return;
  }

  int    statusCode = doc["status"]      | 0;
  String code       = doc["code"]        | "";
  String desc       = doc["description"] | "";

  // ── Token invalide ───────────────────────────────────────────────
  if (code == "INVALID_TOKEN") {
    PlaySound(4); // TTS: "Token invalide"
    oledShowMessage("Token invalide!", 2, true, 1000);
    Serial.println("[CheckProf] " + desc);
    return;
  }

  // ── UID manquant ─────────────────────────────────────────────────
  if (code == "MISSING_UID") {
    PlaySound(4); // TTS: "UID manquant"
    oledShowMessage("UID manquant!", 2, true, 1000);
    Serial.println("[CheckProf] " + desc);
    return;
  }

  // ── Professeur non reconnu ───────────────────────────────────────
  if (code == "PROFESSOR_NOT_FOUND") {
    PlaySound(5); // TTS: "Professeur non reconnu"
    showRedAlert(1500);
    PlaySound(2); // TTS: "Accès refusé"
    oledShowMessage("Professeur\nnon reconnu", 2, true, 800);
    Serial.println("[CheckProf] " + desc);
    return;
  }

  // ── Erreur serveur ───────────────────────────────────────────────
  if (code == "SERVER_ERROR") {
    PlaySound(4); // TTS: "Erreur serveur"
    oledShowMessage("Erreur serveur!", 2, true, 1000);
    Serial.println("[CheckProf] " + desc);
    return;
  }

  // ── Session ouverte ──────────────────────────────────────────────
  if (code == "SESSION_OPENED") {
    String sessionId = doc["session_id"] | "";

    if (sessionId == "") {
      PlaySound(4); // TTS: "Session ID manquant"
      oledShowMessage("Session ID\nmanquant!", 2, true, 1000);
      Serial.println("[CheckProf] SESSION_OPENED but no session_id in response");
      return;
    }

    PlaySound(3); // TTS: "Accès accordé"
    sessionActive    = true;
    sessionStartTime = millis();
    sessionUid       = cardUid;
    fingerWakeUp();
    oledShowMessage("Accès accordé\n" + sessionId, 1, true, 2000);
    handleEnrollAndFinger(sessionId, btn);
    return;
  }

  // ── Code inattendu ───────────────────────────────────────────────
  PlaySound(4); // TTS: "Réponse inattendue"
  oledShowMessage("Réponse\ninattendue!", 2, true, 1000);
  Serial.println("[CheckProf] Unknown code: " + code);
}