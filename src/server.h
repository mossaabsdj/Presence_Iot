#ifndef SERVER_H
#define SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

class WiFiServerManager {
public:
    WiFiServerManager();
    void begin();
    void handle();  // يجب استدعاؤها داخل loop

private:
    WebServer server;
    String wifiSSID;
    String wifiPassword;

    void startAP();
    void startStation();
    void handleRoot();
    void handleSave();
    void loadWiFiCredentials();
    void saveWiFiCredentials(const String &ssid, const String &password);
};

#endif
