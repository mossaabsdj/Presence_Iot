#include "finger.h"
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include "oled.h"

static Adafruit_Fingerprint *fp = nullptr;
static bool fp_ok = false;
static Preferences prefs;
static HardwareSerial *fingerSerialPtr = nullptr; // ← saves serial reference

bool fingerWaitForTouch() {
  if (!fp_ok || fp == nullptr) {
    oledShowMessage("Scanner\nla carte", 2, true, 0);
    return false;
  }

  oledShowMessage("Poser\nle doigt", 2, true, 0);

  while (true) {
    uint8_t p = fp->getImage();
    if (p == FINGERPRINT_OK) {
      oledShowMessage("Doigt\ndétecté", 2, true, 500);
      return true;
    }
    delay(100);
  }
}

bool fingerDetected() {
  if (!fp_ok || fp == nullptr) return false;

  uint8_t p = fp->getImage();
  if (p == FINGERPRINT_OK) {
    oledShowMessage("Doigt détecté", 2, true, 500);
    return true;
  }

  return false;
}

bool fingerBegin(HardwareSerial &ser, int rxPin, int txPin, uint32_t baud) {
  fingerSerialPtr = &ser; // ← save reference for sleep/wake

  Serial.println("=== Initialisation empreinte (MODE BOUCLE) ===");

  uint32_t bauds[] = {57600};
  int numBauds = 1;

  while (true) {

    Serial.println(">> Test broches RX/TX normales...");
    for (int i = 0; i < numBauds; i++) {
      Serial.print("   Baud: "); Serial.println(bauds[i]);

      ser.end();
      ser.begin(bauds[i], SERIAL_8N1, rxPin, txPin);
      delay(200);

      static Adafruit_Fingerprint fpStatic(&ser);
      fp = &fpStatic;

      if (fp->verifyPassword()) {
        Serial.print("✓ TROUVÉ! Baud = "); Serial.println(bauds[i]);
        Serial.print("   RX = "); Serial.print(rxPin);
        Serial.print(" | TX = "); Serial.println(txPin);

        prefs.begin("finger_uid", false);
        fp_ok = true;
        oledShowMessage("CAPTEUR OK");

        Serial.println("=======================");
        return true;
      }
    }

    Serial.println(">> Échec. Test broches RX/TX inversées...");
    for (int i = 0; i < numBauds; i++) {
      Serial.print("   Baud: "); Serial.println(bauds[i]);

      ser.end();
      ser.begin(bauds[i], SERIAL_8N1, txPin, rxPin); // swapped
      delay(200);

      static Adafruit_Fingerprint fpStatic2(&ser);
      fp = &fpStatic2;

      if (fp->verifyPassword()) {
        Serial.print("✓ TROUVÉ! Baud = "); Serial.println(bauds[i]);
        Serial.print("   RX = "); Serial.print(txPin);
        Serial.print(" | TX = "); Serial.println(rxPin);

        Serial.println("⚠️ RX/TX INVERSÉS — vérifier le câblage!");

        prefs.begin("finger_uid", false);
        fp_ok = true;
        oledShowMessage("CAPTEUR OK");

        Serial.println("=======================");
        return true;
      }
    }

    Serial.println("✗ ÉCHEC — nouvelle tentative dans 2 secondes...");
    fp_ok = false;
    delay(2000);
  }
}

bool fingerIsOk() {
  return fp_ok && fp != nullptr;
}

int fingerScanMatchId() {
  if (!fingerIsOk()) {
    oledShowMessage("Capteur\nnon prêt", 2, true, 300);
    return -1;
  }

  oledShowMessage("Attente\ndu doigt", 2, true, 0);
  if (fp->getImage() != FINGERPRINT_OK) return -1;

  oledShowMessage("Lecture\nen cours...", 2, true, 0);
  if (fp->image2Tz() != FINGERPRINT_OK) {
    oledShowMessage("Erreur\nlecture", 2, true, 300);
    return -1;
  }

  oledShowMessage("Recherche...", 2, true, 0);
  if (fp->fingerSearch() != FINGERPRINT_OK) return -1;

  oledShowMessage("Correspondance!", 2, true, 300);
  return (int)fp->fingerID;
}

int fingerEnrollNew() {
  if (!fingerIsOk()) return -1;

  int id = -1;

  for (int i = 1; i <= 127; i++) {
    String dummy;
    if (!fingerGetLinkedUid(i, dummy)) {
      id = i;
      break;
    }
  }

  if (id == -1) {
    oledShowMessage("Aucun ID libre", 2, true, 1000);
    return -1;
  }

  oledShowMessage("Inscription\nID: " + String(id), 2, true, 1000);

  // -------- Étape 1 --------
  oledShowMessage("Poser le doigt\nÉtape 1", 2, true, 0);
  while (fp->getImage() != FINGERPRINT_OK) delay(100);

  if (fp->image2Tz(1) != FINGERPRINT_OK) {
    oledShowMessage("Erreur lecture\nÉtape 1", 2, true, 1000);
    return -1;
  }

  oledShowMessage("Retirer\nle doigt", 2, true, 1500);
  while (fp->getImage() != FINGERPRINT_NOFINGER) delay(100);

  // -------- Étape 2 --------
  oledShowMessage("Poser le doigt\nÉtape 2", 2, true, 0);
  while (fp->getImage() != FINGERPRINT_OK) delay(100);

  if (fp->image2Tz(2) != FINGERPRINT_OK) {
    oledShowMessage("Erreur lecture\nÉtape 2", 2, true, 1000);
    return -1;
  }

  // -------- Créer + Stocker --------
  if (fp->createModel() != FINGERPRINT_OK) {
    oledShowMessage("Doigts différents", 2, true, 1500);
    return -1;
  }

  if (fp->storeModel(id) != FINGERPRINT_OK) {
    oledShowMessage("Échec stockage", 2, true, 2000);
    return -1;
  }

  oledShowMessage("Inscrit ID: " + String(id), 2, true, 1500);
  return id;
}

bool fingerLinkUid(int fingerId, const String &uid) {
  if (fingerId < 0) return false;

  char key[16];
  snprintf(key, sizeof(key), "id_%d", fingerId);

  return prefs.putString(key, uid) > 0;
}

bool fingerGetLinkedUid(int fingerId, String &uidOut) {
  uidOut = "";
  if (fingerId < 0) return false;

  char key[16];
  snprintf(key, sizeof(key), "id_%d", fingerId);

  uidOut = prefs.getString(key, "");
  return uidOut.length() > 0;
}

void fingerSleep() {
  if (!fp_ok || fp == nullptr || fingerSerialPtr == nullptr) return;

  uint8_t packet[] = {
    0xEF, 0x01,             // Header
    0xFF, 0xFF, 0xFF, 0xFF, // Adresse par défaut
    0x01,                   // Paquet commande
    0x00, 0x03,             // Longueur
    0x33,                   // Commande: veille
    0x00, 0x37              // Checksum
  };
  fingerSerialPtr->write(packet, sizeof(packet));
  delay(100);
}

void fingerWakeUp() {
  if (!fp_ok || fp == nullptr) return;

  fp->verifyPassword(); // n'importe quel paquet valide réveille le capteur AS608
  delay(200);
}