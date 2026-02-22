#include "finger.h"
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include "oled.h"

static Adafruit_Fingerprint *fp = nullptr;
static bool fp_ok = false;
static Preferences prefs;
bool fingerWaitForTouch() {
  if (!fp_ok || fp == nullptr) {
    oledShowMessage("Scan UID\nCard", 2, true, 0);
    return false;
  }

  oledShowMessage("Place\nFinger", 2, true, 0);

  while (true) {
    uint8_t p = fp->getImage();

    if (p == FINGERPRINT_OK) {
      oledShowMessage("Finger\nDetected", 2, true, 500);
      return true;
    }

    delay(100);
  }
}
bool fingerDetected() {
    if (!fp_ok || fp == nullptr) return false;

    uint8_t p = fp->getImage();
    if (p == FINGERPRINT_OK) {
        oledShowMessage("Finger Detected", 2, true, 500);
        return true;
    }

    return false;
}
bool fingerBegin(HardwareSerial &ser, int rxPin, int txPin, uint32_t baud) {
  ser.begin(baud, SERIAL_8N1, rxPin, txPin);

  static Adafruit_Fingerprint fpStatic(&ser);
  fp = &fpStatic;

  // افتح مساحة تخزين NVS
  prefs.begin("finger_uid", false);

  fp_ok = fp->verifyPassword();
  if (fp_ok) {
   oledShowMessage("FINGER OK");
  } else {
   oledShowMessage("SCAN UID CARD");
  }
  return fp_ok;
}

bool fingerIsOk() {
  return fp_ok && fp != nullptr;
}

int fingerScanMatchId() {
  if (!fingerIsOk()) {
    oledShowMessage("Finger\nNot Ready", 2, true, 300);
    return -1;
  }
     

  // 1) انتظار/قراءة الصورة
  oledShowMessage("Waiting\nFinger", 2, true, 0);
  if (fp->getImage() != FINGERPRINT_OK) {
    return -1;
  }
      

  // 2) تحويل الصورة
  oledShowMessage("Reading\nFinger...", 2, true, 0);
  if (fp->image2Tz() != FINGERPRINT_OK) {
    oledShowMessage("Read\nError", 2, true, 300);
    return -1;
  }
     

  // 3) البحث عن مطابقة
  oledShowMessage("Matching...", 2, true, 0);
  if (fp->fingerSearch() != FINGERPRINT_OK) {
    oledShowMessage("No\nMatch", 2, true, 300);
    return -1;
  }

  // 4) نجحت المطابقة
  oledShowMessage("Matched!", 2, true, 300);

  return (int)fp->fingerID;
}
int fingerEnrollNew() {
  if (!fingerIsOk()) return -1;

  int id = -1;

  // 1️⃣ إيجاد ID فارغ
  for (int i = 1; i <= 1200; i++) { // IDs 1-127
    String dummy;
    if (!fingerGetLinkedUid(i, dummy)) {
      id = i;
      break;
    }
  }

  if (id == -1) {
    oledShowMessage("No empty ID", 2, true, 1000);
    return -1;
  }

  oledShowMessage("Enroll Finger\nID: " + String(id), 2, true, 1000);

  // 2️⃣ تسجيل البصمة يحتاج 2-3 مرات
  for (int step = 1; step <= 3; step++) {
    oledShowMessage("Place finger\nStep " + String(step), 2, true, 0);

    int p = -1;
    while (p != FINGERPRINT_OK) {
      p = fp->getImage();
      delay(100);
    }

    if (fp->image2Tz(step) != FINGERPRINT_OK) {
      oledShowMessage("Read Error\nStep " + String(step), 2, true, 300);
      return -1;
    }

    if (step < 3) {
      oledShowMessage("Lift finger\nand place again", 2, true, 1000);
      delay(500);
    }
  }

  // 3️⃣ تخزين البصمة في ID
  if (fp->createModel() != FINGERPRINT_OK) {
    oledShowMessage("Create Model\nFailed", 2, true, 1000);
    return -1;
  }

  if (fp->storeModel(id) != FINGERPRINT_OK) {
    oledShowMessage("Store Failed", 2, true, 2000);
    return -1;
  }

  oledShowMessage("Enrolled ID: " + String(id), 2, true, 1000);
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
