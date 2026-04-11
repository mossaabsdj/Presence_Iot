#include "data.h"
#include <Preferences.h>

Preferences prefs;

// 🔒 default values (first boot only)
static int sall = 22;
static unsigned long sessionDelay = 5000; // 5 seconds default
static String serverIP = "192.168.1.42";  // default server IP
static String token = "shewr1224";        // default token

// ---------------------------
// Initialize data from Preferences
// ---------------------------
void dataInit() {
  prefs.begin("config", false);

  sall         = prefs.getInt("sall", sall);
  sessionDelay = prefs.getULong("sessionDelay", sessionDelay);
  serverIP     = prefs.getString("serverIP", serverIP);
  token        = prefs.getString("token", token);  // ✅ load stored token
}

// ===== SALL =====
int getSall() {
  return sall;
}

void setSall(int newSall) {
  sall = newSall;
  prefs.putInt("sall", sall);
}

// ===== SESSION DELAY =====
unsigned long getSessionDelay() {
  return sessionDelay;
}

void setSessionDelay(unsigned long newDelay) {
  sessionDelay = newDelay;
  prefs.putULong("sessionDelay", sessionDelay);
}

// ===== SERVER IP =====
String getServerIP() {
  return serverIP;
}

void setServerIP(const String& newIP) {
  serverIP = newIP;
  prefs.putString("serverIP", serverIP);
}

// ===== TOKEN =====          // ✅ added
String getToken() {
  return token;
}

void setToken(const String& newToken) {
  token = newToken;
  prefs.putString("token", token);
}