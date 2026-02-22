#include "server.h"
#include <EEPROM.h>
#include "oled.h"

#define EEPROM_SIZE 96

WiFiServerManager::WiFiServerManager() : server(80) {
    wifiSSID = "iPhone";
    wifiPassword = "11111111";
}

void WiFiServerManager::begin() {
    EEPROM.begin(EEPROM_SIZE);
    loadWiFiCredentials();

    if (wifiSSID == "") {
        startAP();
    } else {
        startStation();
    }
}

void WiFiServerManager::handle() {
    server.handleClient();
}

// ===== تحميل وحفظ الإعدادات =====
void WiFiServerManager::saveWiFiCredentials(const String &ssid, const String &password) {
    for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0); // مسح البيانات القديمة
    for (int i = 0; i < ssid.length(); i++) EEPROM.write(i, ssid[i]);
    for (int i = 0; i < password.length(); i++) EEPROM.write(32 + i, password[i]);
    EEPROM.commit();
}

void WiFiServerManager::loadWiFiCredentials() {
    char ssid[32] = {0};
    char pass[64] = {0};
    for (int i = 0; i < 32; i++) ssid[i] = EEPROM.read(i);
    for (int i = 0; i < 64; i++) pass[i] = EEPROM.read(32 + i);
    wifiSSID = String(ssid);
    wifiPassword = String(pass);
}

// ===== وضع AP مؤقت =====
void WiFiServerManager::startAP() {
    oledShowMessage("AP Mode WiFi Setup", 2, true, 1000);
    WiFi.softAP("ESP32");
    oledShowMessage("AP IP:\n" + WiFi.softAPIP().toString(), 2, true, 20000);

    server.on("/", std::bind(&WiFiServerManager::handleRoot, this));
    server.on("/save", HTTP_POST, std::bind(&WiFiServerManager::handleSave, this));

    server.begin();
    oledShowMessage("HTTP Server Started", 2, true, 1000);
}

// ===== وضع Station =====
void WiFiServerManager::startStation() {
    oledShowMessage("Connecting to Wi-Fi:\n" + wifiSSID, 2, true, 1000);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        oledShowMessage("Connecting... " + String(attempts + 1), 2, true, 200);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        oledShowMessage("Connected!\nIP:\n" + WiFi.localIP().toString(), 2, true, 2000);
    } else {
        oledShowMessage("Failed to connect\nStarting AP...", 2, true, 2000);
        wifiSSID = "";
        startAP();
    }
}

// ===== صفحة HTML =====
void WiFiServerManager::handleRoot() {
    const char* htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <head>
        <title>ESP32 WiFi Setup</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
          body { font-family: Arial; text-align:center; margin-top:50px; }
          input { padding: 8px; width:200px; margin:5px; }
          button { padding:10px 20px; }
        </style>
      </head>
      <body>
        <h2>Configure WiFi</h2>
        <form action="/save" method="POST">
          <input type="text" name="ssid" placeholder="SSID"><br>
          <input type="password" name="pass" placeholder="Password"><br>
          <button type="submit">Save & Connect</button>
        </form>
      </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", htmlPage);
}

void WiFiServerManager::handleSave() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        wifiSSID = server.arg("ssid");
        wifiPassword = server.arg("pass");

        saveWiFiCredentials(wifiSSID, wifiPassword);

        oledShowMessage("Credentials Saved!\nRestarting...", 2, true, 2000);
        delay(2000);
        ESP.restart(); // إعادة التشغيل للاتصال بالشبكة الجديدة
    } else {
        oledShowMessage("Missing SSID or Password", 2, true, 2000);
    }
}
