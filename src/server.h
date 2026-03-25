#pragma once
#include <WiFi.h>
#include <WebServer.h>

class WiFiServerManager {
public:
    WiFiServerManager();
    void begin();
    void handle();

private:
    WebServer server;
    String wifiSSID;
    String wifiPassword;

    void startAP();
    void startStation();
    void registerRoutes();   // ← add this line
    void saveWiFiCredentials(const String &ssid, const String &password);
    void loadWiFiCredentials();

    // Pages
    void handleRoot();
    void handleSave();
    void handleConfig();
    void handleSetSall();
    void handleSetDelay();
void handleSetServerIP();
    // API endpoints
    void handleApiStatus();
    void handleApiRestart();
    void handleApiReset();

    // HTML builder
    String buildDashboardHTML(const String& activeTab);
    //sen req
};
    String sendRequest(String serverPath, String uid) ;
