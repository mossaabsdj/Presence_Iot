#include "server.h"
#include <EEPROM.h>
#include "oled.h"
#include "data.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
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

// ===== Save / Load Credentials =====
void WiFiServerManager::saveWiFiCredentials(const String &ssid, const String &password) {
    for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
    for (size_t i = 0; i < ssid.length(); i++) EEPROM.write(i, ssid[i]);
    for (size_t i = 0; i < password.length(); i++) EEPROM.write(32 + i, password[i]);
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
void WiFiServerManager::handleSetToken() {
    if (server.hasArg("token")) {
        String newToken = server.arg("token");
        setToken(newToken);
        oledShowMessage("Token Updated", 2, true, 1500);
        server.sendHeader("Location", "/config");
        server.send(303);
    } else {
        server.send(400, "text/plain", "Missing token value");
    }
}
// ===== AP Mode =====
void WiFiServerManager::startAP() {
    //oledShowMessage("AP Mode WiFi Setup", 2, true, 1000);
    WiFi.softAP("ESP32_" + String(getSall()),"admin123");
    //oledShowMessage("AP IP:\n" + WiFi.softAPIP().toString(), 2, true, 1000);

    server.on("/",           std::bind(&WiFiServerManager::handleRoot,      this));
    server.on("/save",       HTTP_POST, std::bind(&WiFiServerManager::handleSave,      this));
    server.on("/config",     std::bind(&WiFiServerManager::handleConfig,    this));
    server.on("/set-sall",   HTTP_POST, std::bind(&WiFiServerManager::handleSetSall,   this));
    server.on("/set-server-ip", HTTP_POST, std::bind(&WiFiServerManager::handleSetServerIP, this));
    server.on("/api/status", std::bind(&WiFiServerManager::handleApiStatus, this));
    server.on("/set-token", HTTP_POST, std::bind(&WiFiServerManager::handleSetToken, this));  // ✅
    server.on("/api/restart",HTTP_POST, std::bind(&WiFiServerManager::handleApiRestart,this));
    server.on("/api/reset",  HTTP_POST, std::bind(&WiFiServerManager::handleApiReset,  this));
server.on("/set-delay", HTTP_POST,
          std::bind(&WiFiServerManager::handleSetDelay, this));
    server.begin();
   // oledShowMessage("HTTP Server Started", 2, true, 1000);
}

// ===== Station Mode =====
void WiFiServerManager::startStation() {
    oledShowMessage("Connecting to Wi-Fi:\n" + wifiSSID, 2, true, 200);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        oledShowMessage("Connecting... " + String(attempts + 1), 2, true, 200);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        oledShowMessage("Connected!\nIP:\n" + WiFi.localIP().toString(), 1, true, 800);
         // start mDNS
    if (MDNS.begin("iot")) {
          MDNS.addService("http", "tcp", 80);

        Serial.println("mDNS started");
    } else {
        Serial.println("mDNS failed");
    }
        startAP();
    } else {
        oledShowMessage("Failed to connect\nStarting AP...", 2, true, 800);
        wifiSSID = "";
        
        startAP();
    }
}

// ===== API: System Status (JSON) =====
void WiFiServerManager::handleApiStatus() {
    bool connected = (WiFi.status() == WL_CONNECTED);
    String ip      = connected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    String mode    = connected ? "Station" : "AP";

    String json = "{";
    json += "\"wifi\":{";
    json += "\"ssid\":\"" + wifiSSID + "\",";
    json += "\"mode\":\"" + mode     + "\",";
    json += "\"ip\":\"" + ip + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"connected\":" + String(connected ? "true" : "false");
    json += "},";
    json += "\"system\":{";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"chipModel\":\"" + String(ESP.getChipModel()) + "\",";
    json += "\"cpuFreq\":" + String(ESP.getCpuFreqMHz()) + ",";
    json += "\"flashSize\":" + String(ESP.getFlashChipSize()) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "},";
    json += "\"config\":{";
    json += "\"sall\":" + String(getSall());
    json += "}";
    json += "}";

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

// ===== API: Restart =====
void WiFiServerManager::handleApiRestart() {
    server.send(200, "application/json", "{\"status\":\"restarting\"}");
    delay(500);
    ESP.restart();
}

// ===== API: Factory Reset =====
void WiFiServerManager::handleApiReset() {
    for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
    EEPROM.commit();
    server.send(200, "application/json", "{\"status\":\"reset\"}");
    delay(500);
    ESP.restart();
}

// ===== Root Page (WiFi Setup) =====
void WiFiServerManager::handleRoot() {
    server.send(200, "text/html", buildDashboardHTML("wifi"));
}

// ===== Config Page =====
void WiFiServerManager::handleConfig() {
    server.send(200, "text/html", buildDashboardHTML("config"));
}

// ===== Save WiFi =====
void WiFiServerManager::handleSave() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        wifiSSID     = server.arg("ssid");
        wifiPassword = server.arg("pass");
        saveWiFiCredentials(wifiSSID, wifiPassword);
        oledShowMessage("Credentials Saved!\nRestarting...", 2, true, 2000);
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Missing SSID or Password");
    }
}
//========set ip adress========
void WiFiServerManager::handleSetServerIP() {
    if (server.hasArg("serverIP")) {
        String newIP = server.arg("serverIP");
        setServerIP(newIP);  // save in Preferences
        oledShowMessage("Server IP Updated:\n" + newIP, 2, true, 1500);

        // Redirect back to config page
        server.sendHeader("Location", "/config");
        server.send(303);
    } else {
        server.send(400, "text/plain", "Missing serverIP value");
    }
}
// ===== Set SALL =====
void WiFiServerManager::handleSetSall() {
    if (server.hasArg("sall")) {
        int newSall = server.arg("sall").toInt();
        setSall(newSall);
        oledShowMessage("SALL Updated:\n" + String(newSall), 2, true, 1500);
        server.sendHeader("Location", "/config");
        server.send(303);
    } else {
        server.send(400, "text/plain", "Missing sall value");
    }
}
//======set session delay =====
void WiFiServerManager::handleSetDelay() {
    if (!server.hasArg("delay")) {
        server.send(400, "text/plain", "Missing delay");
        return;
    }

    unsigned long minutes = server.arg("delay").toInt();

    // allow 1 min → 12 hours
    if (minutes < 1 || minutes > 720) {
        server.send(400, "text/plain", "Invalid range");
        return;
    }

    setSessionDelay(minutes * 60UL * 1000UL); // ✅ convert to ms

    oledShowMessage("Delay set:\n" + String(minutes) + " min",
                    2, true, 1500);

    server.sendHeader("Location", "/config");
    server.send(303);
}
// ===== Build Full Dashboard HTML =====
String WiFiServerManager::buildDashboardHTML(const String& activeTab) {
    int sall = getSall();
    String token=getToken();
    unsigned long delayMin = getSessionDelay() / 60000UL;
 String SERVER_IP=getServerIP();
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Dashboard</title>
<link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600;700&family=Syne:wght@400;700;800&display=swap" rel="stylesheet">
<style>
  :root {
    --bg:      #0a0c10;
    --surface: #111318;
    --card:    #181c24;
    --border:  #252a36;
    --accent:  #00e5a0;
    --accent2: #0094ff;
    --warn:    #ff6b35;
    --text:    #e2e8f0;
    --muted:   #6b7280;
    --success: #00e5a0;
    --danger:  #ff4757;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Syne', sans-serif;
    min-height: 100vh;
  }
  /* ── Header ── */
  header {
    display: flex; align-items: center; justify-content: space-between;
    padding: 18px 28px;
    border-bottom: 1px solid var(--border);
    background: var(--surface);
    position: sticky; top: 0; z-index: 100;
  }
  .logo { display: flex; align-items: center; gap: 10px; }
  .logo-dot {
    width: 10px; height: 10px; border-radius: 50%;
    background: var(--accent);
    box-shadow: 0 0 10px var(--accent);
    animation: pulse 2s infinite;
  }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.4} }
  .logo-text { font-size: 1.1rem; font-weight: 800; letter-spacing: .05em; }
  .logo-text span { color: var(--accent); }
  .uptime-badge {
    font-family: 'JetBrains Mono', monospace;
    font-size: .75rem; color: var(--muted);
    background: var(--card); padding: 5px 12px; border-radius: 20px;
    border: 1px solid var(--border);
  }
  /* ── Nav ── */
  nav {
    display: flex; gap: 4px;
    padding: 12px 28px;
    background: var(--surface);
    border-bottom: 1px solid var(--border);
  }
  .tab {
    padding: 8px 18px; border-radius: 8px; cursor: pointer;
    font-size: .85rem; font-weight: 700; letter-spacing: .04em;
    border: none; background: transparent; color: var(--muted);
    transition: all .2s;
  }
  .tab.active { background: var(--accent); color: #000; }
  .tab:not(.active):hover { background: var(--card); color: var(--text); }
  /* ── Layout ── */
  main { max-width: 960px; margin: 0 auto; padding: 28px 20px; }
  .page { display: none; }
  .page.active { display: block; }
  /* ── Status Grid ── */
  .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
  .grid-3 { display: grid; grid-template-columns: repeat(3,1fr); gap: 16px; }
  @media(max-width:600px){ .grid-2,.grid-3{ grid-template-columns:1fr; } }
  /* ── Cards ── */
  .card {
    background: var(--card); border: 1px solid var(--border);
    border-radius: 14px; padding: 20px;
    transition: border-color .2s;
  }
  .card:hover { border-color: var(--accent); }
  .card-header {
    display: flex; align-items: center; justify-content: space-between;
    margin-bottom: 16px;
  }
  .card-title {
    font-size: .7rem; font-weight: 700; letter-spacing: .12em;
    text-transform: uppercase; color: var(--muted);
  }
  .stat-value {
    font-family: 'JetBrains Mono', monospace;
    font-size: 1.6rem; font-weight: 700; color: var(--accent);
    line-height: 1;
  }
  .stat-label { font-size: .78rem; color: var(--muted); margin-top: 4px; }
  /* Badge */
  .badge {
    font-size: .65rem; font-weight: 700; padding: 3px 8px;
    border-radius: 20px; letter-spacing: .08em;
  }
  .badge-green { background: rgba(0,229,160,.15); color: var(--success); border: 1px solid rgba(0,229,160,.3); }
  .badge-red   { background: rgba(255,71,87,.15);  color: var(--danger);  border: 1px solid rgba(255,71,87,.3); }
  .badge-blue  { background: rgba(0,148,255,.15);  color: var(--accent2); border: 1px solid rgba(0,148,255,.3); }
  /* ── Detail List ── */
  .detail-list { display: flex; flex-direction: column; gap: 10px; }
  .detail-row {
    display: flex; justify-content: space-between; align-items: center;
    padding: 8px 0; border-bottom: 1px solid var(--border);
  }
  .detail-row:last-child { border-bottom: none; }
  .detail-key  { font-size: .82rem; color: var(--muted); }
  .detail-val  { font-family: 'JetBrains Mono', monospace; font-size: .82rem; color: var(--text); }
  /* ── Forms ── */
  .form-group { margin-bottom: 14px; }
  label { display: block; font-size: .75rem; font-weight: 700; color: var(--muted); margin-bottom: 6px; letter-spacing: .08em; text-transform: uppercase; }
  input[type=text], input[type=password], input[type=number] {
    width: 100%; padding: 11px 14px;
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 8px; color: var(--text);
    font-family: 'JetBrains Mono', monospace; font-size: .9rem;
    transition: border-color .2s;
    outline: none;
  }
  input:focus { border-color: var(--accent); box-shadow: 0 0 0 3px rgba(0,229,160,.1); }
  /* ── Buttons ── */
  .btn {
    display: inline-flex; align-items: center; gap: 7px;
    padding: 11px 20px; border-radius: 8px; border: none;
    font-family: 'Syne', sans-serif; font-size: .85rem; font-weight: 700;
    cursor: pointer; transition: all .2s; letter-spacing: .04em;
  }
  .btn-primary   { background: var(--accent); color: #000; }
  .btn-primary:hover { background: #00c988; }
  .btn-danger    { background: rgba(255,71,87,.15); color: var(--danger); border: 1px solid rgba(255,71,87,.4); }
  .btn-danger:hover { background: var(--danger); color: #fff; }
  .btn-ghost     { background: var(--card); color: var(--text); border: 1px solid var(--border); }
  .btn-ghost:hover { border-color: var(--accent); color: var(--accent); }
  .btn-full { width: 100%; justify-content: center; }
  .btn-row { display: flex; gap: 10px; flex-wrap: wrap; }
  /* ── RSSI bar ── */
  .rssi-bar-wrap { background: var(--border); border-radius: 4px; height: 6px; margin-top: 8px; overflow: hidden; }
  .rssi-bar { height: 100%; border-radius: 4px; background: var(--accent); transition: width .5s; }
  /* ── Section title ── */
  .section-title { font-size: 1.15rem; font-weight: 800; margin-bottom: 16px; }
  .section-title span { color: var(--accent); }
  /* ── Divider ── */
  .divider { border: none; border-top: 1px solid var(--border); margin: 24px 0; }
  /* ── Toast ── */
  #toast {
    position: fixed; bottom: 24px; right: 24px;
    background: var(--card); border: 1px solid var(--accent);
    color: var(--text); padding: 12px 20px; border-radius: 10px;
    font-size: .85rem; box-shadow: 0 8px 30px rgba(0,0,0,.5);
    transform: translateY(80px); opacity: 0;
    transition: all .3s; z-index: 999;
  }
  #toast.show { transform: translateY(0); opacity: 1; }
  /* ── Spinner ── */
  .spin { animation: spin .8s linear infinite; display: inline-block; }
  @keyframes spin { to { transform: rotate(360deg); } }
</style>
</head>
<body>

<header>
  <div class="logo">
    <div class="logo-dot"></div>
    <div class="logo-text">ESP<span>32</span> Dashboard</div>
  </div>
  <div class="uptime-badge" id="uptime-display">Loading…</div>
</header>

<nav>
  <button class="tab )rawliteral";
    html += (activeTab == "status") ? "active" : "";
    html += R"rawliteral(" onclick="showTab('status')">⬡ Status</button>
  <button class="tab )rawliteral";
    html += (activeTab == "wifi") ? "active" : "";
    html += R"rawliteral(" onclick="showTab('wifi')">⬡ WiFi</button>
  <button class="tab )rawliteral";
    html += (activeTab == "config") ? "active" : "";
    html += R"rawliteral(" onclick="showTab('config')">⬡ Config</button>
  <button class="tab" onclick="showTab('system')">⬡ System</button>
</nav>

<main>

<!-- ══════════ STATUS PAGE ══════════ -->
<div class="page" id="page-status">
  <div class="section-title">System <span>Overview</span></div>
  <div class="grid-3" style="margin-bottom:16px">
    <div class="card">
      <div class="card-header">
        <span class="card-title">WiFi</span>
        <span class="badge" id="s-wifi-badge">—</span>
      </div>
      <div class="stat-value" id="s-wifi-mode">—</div>
      <div class="stat-label" id="s-wifi-ip">Loading…</div>
    </div>
    <div class="card">
      <div class="card-header">
        <span class="card-title">Free Heap</span>
      </div>
      <div class="stat-value" id="s-heap">—</div>
      <div class="stat-label">bytes available</div>
    </div>
    <div class="card">
      <div class="card-header">
        <span class="card-title">SALL Value</span>
      </div>
      <div class="stat-value" id="s-sall">—</div>
      <div class="stat-label">current config</div>
    </div>
  </div>

  <div class="grid-2">
    <div class="card">
      <div class="card-header"><span class="card-title">WiFi Signal</span></div>
      <div class="detail-val" id="s-rssi" style="font-size:1.2rem">— dBm</div>
      <div class="rssi-bar-wrap"><div class="rssi-bar" id="s-rssi-bar" style="width:0%"></div></div>
    </div>
    <div class="card">
      <div class="card-header"><span class="card-title">Chip Info</span></div>
      <div class="detail-list">
        <div class="detail-row"><span class="detail-key">Model</span><span class="detail-val" id="s-chip">—</span></div>
        <div class="detail-row"><span class="detail-key">CPU</span><span class="detail-val" id="s-cpu">—</span></div>
        <div class="detail-row"><span class="detail-key">Flash</span><span class="detail-val" id="s-flash">—</span></div>
      </div>
    </div>
  </div>

  <hr class="divider">
  <div class="btn-row">
    <button class="btn btn-ghost" onclick="loadStatus()">↻ Refresh</button>
    <button class="btn btn-danger" onclick="confirmRestart()">↺ Restart Device</button>
    <button class="btn btn-danger" onclick="confirmReset()">⚠ Factory Reset</button>
  </div>
</div>

<!-- ══════════ WIFI PAGE ══════════ -->
<div class="page" id="page-wifi">
  <div class="section-title">WiFi <span>Setup</span></div>
  <div class="grid-2">
    <div class="card">
      <div class="card-header"><span class="card-title">Current Connection</span></div>
      <div class="detail-list">
        <div class="detail-row"><span class="detail-key">SSID</span><span class="detail-val" id="w-ssid">—</span></div>
        <div class="detail-row"><span class="detail-key">Mode</span><span class="detail-val" id="w-mode">—</span></div>
        <div class="detail-row"><span class="detail-key">IP</span><span class="detail-val" id="w-ip">—</span></div>
        <div class="detail-row"><span class="detail-key">RSSI</span><span class="detail-val" id="w-rssi">—</span></div>
      </div>
    </div>
    <div class="card">
      <div class="card-header"><span class="card-title">Configure New Network</span></div>
      <form action="/save" method="POST">
        <div class="form-group">
          <label>Network SSID</label>
          <input type="text" name="ssid" placeholder="Enter network name" required>
        </div>
        <div class="form-group">
          <label>Password</label>
          <input type="password" name="pass" placeholder="Enter password">
        </div>
        <button type="submit" class="btn btn-primary btn-full">Save &amp; Connect</button>
      </form>
    </div>
  </div>
</div>

<!-- ══════════ CONFIG PAGE ══════════ -->
<div class="page" id="page-config">
  <div class="section-title">Device <span>Configuration</span></div>
  <div class="grid-2">

    <!-- Session Settings Card -->
    <div class="card">
      <div class="card-header"><span class="card-title">Session Settings</span></div>
      <div class="detail-list" style="margin-bottom:16px">
        <div class="detail-row">
          <span class="detail-key">Current SALL</span>
          <span class="detail-val" id="c-sall-current" style="color:var(--accent);font-size:1.1rem">%SALL%</span>
        </div>
        <div class="detail-row">
          <span class="detail-key">Current Session Delay</span>
          <span class="detail-val" style="color:var(--accent);font-size:1.1rem">%DELAY% min</span>
        </div>
      </div>

      <!-- Update SALL -->
      <form action="/set-sall" method="POST" class="form-group">
        <label>New SALL Value</label>
        <input type="number" name="sall" value="%SALL%" required>
        <button type="submit" class="btn btn-primary btn-full">Update SALL</button>
      </form>

      <!-- Update Session Delay -->
      <form action="/set-delay" method="POST" class="form-group" style="margin-top:12px">
        <label>Session Delay (minutes)</label>
        <input type="number" name="delay" min="1" max="720" value="%DELAY%" required>
        <button type="submit" class="btn btn-primary btn-full">Update Delay</button>
      </form>

      <!-- Update Server IP -->
<form action="/set-server-ip" method="POST" class="form-group" style="margin-top:12px">
  <label>Server IP</label>
  <input type="text" name="serverIP" value="%SERVER_IP%" required>
  <button type="submit" class="btn btn-primary btn-full">Update Server IP</button>
</form>
<form action="/set-token" method="POST" class="form-group" style="margin-top:12px">
  <label>Device Token</label>
  <input type="text" name="token" value="%TOKEN%" required>
  <button type="submit" class="btn btn-primary btn-full">Update Token</button>
</form>
    </div>

    <!-- Device Actions Card -->
    <div class="card">
      <div class="card-header"><span class="card-title">Device Actions</span></div>
      <div class="detail-list">
        <div class="detail-row">
          <span class="detail-key">Restart ESP32</span>
          <button class="btn btn-ghost" onclick="confirmRestart()" style="padding:6px 14px;font-size:.8rem">Restart</button>
        </div>
        <div class="detail-row">
          <span class="detail-key">Factory Reset (clears WiFi)</span>
          <button class="btn btn-danger" onclick="confirmReset()" style="padding:6px 14px;font-size:.8rem">Reset</button>
        </div>
      </div>
    </div>

  </div>
</div>
<!-- ══════════ SYSTEM PAGE ══════════ -->
<div class="page" id="page-system">
  <div class="section-title">System <span>Details</span></div>
  <div class="card">
    <div class="card-header"><span class="card-title">Full Diagnostics</span></div>
    <div class="detail-list" id="system-details">
      <div class="detail-row"><span class="detail-key">Chip Model</span><span class="detail-val" id="d-chip">—</span></div>
      <div class="detail-row"><span class="detail-key">CPU Frequency</span><span class="detail-val" id="d-cpu">—</span></div>
      <div class="detail-row"><span class="detail-key">Flash Size</span><span class="detail-val" id="d-flash">—</span></div>
      <div class="detail-row"><span class="detail-key">Free Heap</span><span class="detail-val" id="d-heap">—</span></div>
      <div class="detail-row"><span class="detail-key">Uptime</span><span class="detail-val" id="d-uptime">—</span></div>
      <div class="detail-row"><span class="detail-key">WiFi SSID</span><span class="detail-val" id="d-ssid">—</span></div>
      <div class="detail-row"><span class="detail-key">WiFi Mode</span><span class="detail-val" id="d-mode">—</span></div>
      <div class="detail-row"><span class="detail-key">IP Address</span><span class="detail-val" id="d-ip">—</span></div>
      <div class="detail-row"><span class="detail-key">Signal (RSSI)</span><span class="detail-val" id="d-rssi">—</span></div>
      <div class="detail-row"><span class="detail-key">SALL</span><span class="detail-val" id="d-sall">—</span></div>
    </div>
  </div>
  <hr class="divider">
  <button class="btn btn-ghost" onclick="loadStatus()">↻ Refresh All</button>
</div>

</main>

<div id="toast"></div>

<script>
// ── Tab navigation ──
function showTab(name) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById('page-' + name).classList.add('active');
  event.target.classList.add('active');
  loadStatus();
}

// ── Toast ──
function toast(msg, duration=3000) {
  const t = document.getElementById('toast');
  t.textContent = msg; t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), duration);
}

// ── Format uptime ──
function fmtUptime(s) {
  const h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sec = s%60;
  return `${h}h ${m}m ${sec}s`;
}

// ── Format bytes ──
function fmtBytes(b) {
  if(b > 1048576) return (b/1048576).toFixed(1)+' MB';
  if(b > 1024)    return (b/1024).toFixed(1)+' KB';
  return b+' B';
}

// ── Load status from API ──
async function loadStatus() {
  try {
    const r = await fetch('/api/status');
    const d = await r.json();

    // Header uptime
    document.getElementById('uptime-display').textContent = 'UP ' + fmtUptime(d.system.uptime);

    // Status page
    const connected = d.wifi.connected;
    document.getElementById('s-wifi-badge').textContent  = connected ? 'ONLINE' : 'AP';
    document.getElementById('s-wifi-badge').className    = 'badge ' + (connected ? 'badge-green' : 'badge-blue');
    document.getElementById('s-wifi-mode').textContent   = d.wifi.mode;
    document.getElementById('s-wifi-ip').textContent     = d.wifi.ip;
    document.getElementById('s-heap').textContent        = fmtBytes(d.system.heap);
    document.getElementById('s-sall').textContent        = d.config.sall;
    const rssiPct = Math.min(100, Math.max(0, 2*(d.wifi.rssi+100)));
    document.getElementById('s-rssi').textContent        = d.wifi.rssi + ' dBm';
    document.getElementById('s-rssi-bar').style.width    = rssiPct + '%';
    document.getElementById('s-chip').textContent        = d.system.chipModel;
    document.getElementById('s-cpu').textContent         = d.system.cpuFreq + ' MHz';
    document.getElementById('s-flash').textContent       = fmtBytes(d.system.flashSize);

    // WiFi page
    document.getElementById('w-ssid').textContent = d.wifi.ssid || '(none)';
    document.getElementById('w-mode').textContent = d.wifi.mode;
    document.getElementById('w-ip').textContent   = d.wifi.ip;
    document.getElementById('w-rssi').textContent = d.wifi.rssi + ' dBm';

    // Config page
    document.getElementById('c-sall-current').textContent = d.config.sall;

    // System page
    document.getElementById('d-chip').textContent   = d.system.chipModel;
    document.getElementById('d-cpu').textContent    = d.system.cpuFreq + ' MHz';
    document.getElementById('d-flash').textContent  = fmtBytes(d.system.flashSize);
    document.getElementById('d-heap').textContent   = fmtBytes(d.system.heap);
    document.getElementById('d-uptime').textContent = fmtUptime(d.system.uptime);
    document.getElementById('d-ssid').textContent   = d.wifi.ssid || '(none)';
    document.getElementById('d-mode').textContent   = d.wifi.mode;
    document.getElementById('d-ip').textContent     = d.wifi.ip;
    document.getElementById('d-rssi').textContent   = d.wifi.rssi + ' dBm';
    document.getElementById('d-sall').textContent   = d.config.sall;

  } catch(e) { toast('⚠ Failed to load status'); }
}

// ── Restart ──
async function confirmRestart() {
  if(!confirm('Restart the ESP32?')) return;
  await fetch('/api/restart', {method:'POST'});
  toast('↺ Restarting…');
}

// ── Factory Reset ──
async function confirmReset() {
  if(!confirm('Factory reset? This will erase WiFi credentials!')) return;
  await fetch('/api/reset', {method:'POST'});
  toast('⚠ Resetting…');
}

// ── Init ──
)rawliteral";
    html += "const initTab = '" + activeTab + "';\n";
    html += R"rawliteral(
document.getElementById('page-' + initTab).classList.add('active');
loadStatus();
setInterval(loadStatus, 5000);
</script>
</body>
</html>)rawliteral";
html.replace("%SALL%", String(sall));
html.replace("%SERVER_IP%", SERVER_IP);
html.replace("%DELAY%", String(delayMin));
html.replace("%TOKEN%",     String(token));  // ✅

    return html;
}






String sendRequest(String serverPath, String uid, int sall, unsigned long sessionDelay, String token) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return "ERROR_WIFI";
  }

  HTTPClient http;
  http.begin(serverPath);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-token", token);

  String jsonBody = "{";
  jsonBody += "\"uid\":\"" + uid + "\",";
  jsonBody += "\"sall\":" + String(sall) + ",";
  jsonBody += "\"sessionDelay\":" + String(sessionDelay);
  jsonBody += "}";

  int httpResponseCode = http.POST(jsonBody);

  if (httpResponseCode > 0) {
    String response = http.getString();
    response.trim();
    http.end();
    return response;                // return raw JSON, don't strip quotes
  } else {
    Serial.println("HTTP Error: " + String(httpResponseCode));
    http.end();
    if (httpResponseCode == -1)  return "ERROR_CONNECT";
    if (httpResponseCode == 404) return "ERROR_404";
    return "ERROR";
  }
}
  

String sendRequestEtudiant(String serverPath, String uid, String Session_ID,String token) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return "ERROR_WIFI";
  }

  HTTPClient http;
  http.begin(serverPath);
  http.addHeader("Content-Type", "application/json");
http.addHeader("x-device-token", token);

  String jsonBody = "{";
  jsonBody += "\"uid\":\"" + uid + "\",";
  jsonBody += "\"Session_ID\":\"" + Session_ID + "\"";
  jsonBody += "\"token\":" + String(token);
  jsonBody += "}";

  int httpResponseCode = http.POST(jsonBody);

  if (httpResponseCode > 0) {
    String response = http.getString();

    response.trim();
    response.replace("\"", "");
    response.replace("\n", "");
    response.replace("\r", "");
    http.end();
    return response;

  } else {
    Serial.println("HTTP Error: " + String(httpResponseCode));
    http.end();
    return "ERROR";
  }
}
