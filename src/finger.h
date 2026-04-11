#pragma once
#include <Arduino.h>

// يرجّع true إذا السنسور راه متصل ويتجاوب
bool fingerBegin(HardwareSerial &ser, int rxPin, int txPin, uint32_t baud = 57600);

// هل السنسور موجود؟
bool fingerIsOk();
void fingerSleep();
void fingerWakeUp();
// يمسح بصمة ويبحث عليها في المكتبة
// يرجّع:
//  >=0 : FingerID (مطابقة)
//  -1  : ماكانش finger / فشل
bool fingerDetected();
int fingerScanMatchId();
int fingerEnrollNew();
// ربط FingerID بـ UID (تاع RFID) وتخزينه
bool fingerLinkUid(int fingerId, const String &uid);

// جلب UID مربوط بـ FingerID (إذا موجود)
// يرجّع true إذا لقا UID
bool fingerGetLinkedUid(int fingerId, String &uidOut);
bool fingerWaitForTouch();
