#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "RTClib.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>


#define RST_PIN       4
#define SS_PIN        5
#define BUTTON_PIN    32
#define GREEN_LED_PIN 15
#define RED_LED_PIN   2
#define BUZZER_PIN    27
#define SDA_PIN       21
#define SCL_PIN       22
#define SCK_PIN       18
#define MOSI_PIN      23
#define MISO_PIN      19


/***** INITIALISATION OF INITIAL VARIABLES****/
MFRC522 mfrc522(SS_PIN, RST_PIN);
WebServer server(80);
RTC_DS3231 rtc;

DateTime now;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

String server_route = "https://rfid.pythonanywhere.com/upload";

const char* USERS_DB = "/users.csv";

unsigned long lastSync = 0;
bool wifiConnected = false;


const char* AP_SSID = "ESP32-RFID";
const char* AP_PASS = "12345678";

String lastScannedUID = "";
String pendingUID = "";
bool SaveMode = false;



void showMessage(String l1, String l2 = "", String l3 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println(l1);
  if (l2.length()) display.println(l2);
  if (l3.length()) display.println(l3);
  display.display();
}


void startAP() {
  String currentApPass = AP_PASS;

  
  if (LittleFS.exists("/ap_config.json")) {
    File f = LittleFS.open("/ap_config.json", FILE_READ);
    if (f) {
      StaticJsonDocument<256> doc;
      if (!deserializeJson(doc, f)) {
        currentApPass = doc["ap_pass"].as<String>();
      }
      f.close();
    }
  }

  WiFi.mode(WIFI_AP);
  
  if (currentApPass.length() < 8) currentApPass = AP_PASS; 

  WiFi.softAP(AP_SSID, currentApPass.c_str());
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}


void handleWifiPage() {
  String options = "";
  String savedList = "";
  
  if (LittleFS.exists("/wifi.json")) {
    File f = LittleFS.open("/wifi.json", FILE_READ);
    if (f) {
      StaticJsonDocument<1024> doc;
      if (!deserializeJson(doc, f)) {
        JsonArray networks = doc.as<JsonArray>();
        for (JsonObject net : networks) {
          String ssid = net["ssid"].as<String>();
          // Build dropdown options
          options += "<option value='" + ssid + "'>" + ssid + "</option>";
          
          // Build management list
          savedList += "<div class='row'><span>" + ssid + "</span>";
          savedList += "<form action='/deleteWifi' method='POST' style='margin:0;'>";
          savedList += "<input type='hidden' name='ssid' value='" + ssid + "'>";
          savedList += "<button type='submit' class='del-btn'>Delete</button></form></div>";
        }
      }
      f.close();
    }
  }

  if (savedList == "") {
    savedList = "<div class='muted' style='text-align:center;'>No networks saved yet.</div>";
  }

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WiFi Setup & Sync</title>
  <style>
    body { font-family: system-ui, -apple-system, sans-serif; background: #0f172a; color: #e5e7eb; margin: 0; padding: 16px; display: flex; justify-content: center; min-height: 100vh; }
    .card { width: 100%; max-width: 400px; background: #020617; border-radius: 12px; padding: 24px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); border: 1px solid #1f2933; }
    h2 { margin: 0 0 20px 0; color: #93c5fd; text-align: center; }
    h3 { margin: 24px 0 12px 0; color: #94a3b8; font-size: 16px; border-bottom: 1px solid #1f2933; padding-bottom: 8px;}
    label { display: block; margin-bottom: 8px; font-size: 14px; color: #94a3b8; font-weight: 500; }
    input, select { width: 100%; padding: 12px; margin-bottom: 20px; border-radius: 8px; border: 1px solid #334155; background: #1e293b; color: #f8fafc; font-size: 16px; box-sizing: border-box; }
    input:focus, select:focus { outline: none; border-color: #3b82f6; }
    button { width: 100%; padding: 14px; background: #2563eb; color: white; border: none; border-radius: 8px; font-weight: 600; font-size: 16px; cursor: pointer; transition: background 0.2s; }
    button:hover { background: #1d4ed8; }
    .del-btn { background: #7f1d1d; padding: 8px 12px; font-size: 12px; width: auto; border-radius: 6px; }
    .del-btn:hover { background: #991b1b; }
    .row { display: flex; justify-content: space-between; align-items: center; padding: 10px; border: 1px solid #1f2933; border-radius: 8px; margin-bottom: 8px; background: #0f172a; }
    .divider { text-align: center; margin: 20px 0; color: #64748b; font-size: 12px; text-transform: uppercase; letter-spacing: 1px; }
    a { display: block; text-align: center; margin-top: 20px; color: #64748b; text-decoration: none; font-size: 14px; }
    .muted { opacity: 0.7; font-size: 14px; }
  </style>
  <script>
    function toggleNewNetwork() {
      var select = document.getElementById("saved_ssid");
      var newForm = document.getElementById("new_network_form");
      if(select.value === "NEW") {
        newForm.style.display = "block";
      } else {
        newForm.style.display = "none";
      }
    }
  </script>
  </head>
  <body>
    <div class="card">
      <h2>Sync via WiFi</h2>
      <form action="/saveWifi" method="POST">
        <label>Select Saved Network</label>
        <select name="saved_ssid" id="saved_ssid" onchange="toggleNewNetwork()">
          <option value="" disabled selected>-- Choose Network --</option>
          )rawliteral" + options + R"rawliteral(
          <option value="NEW">+ Add New Network</option>
        </select>

        <div id="new_network_form" style="display:none;">
          <div class="divider">Or enter new details</div>
          <label>New SSID Name</label>
          <input name="ssid" placeholder="Enter WiFi Name">
          <label>Password</label>
          <input name="pass" type="password" placeholder="Enter WiFi Password">
        </div>

        <button type="submit">Connect & Sync Now</button>
      </form>
      
      <h3>Manage Saved Networks</h3>
      )rawliteral" + savedList + R"rawliteral(

      <a href="/">Cancel & Back to Home</a>
    </div>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleDeleteWifi() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "Missing SSID");
    return;
  }
  
  String targetSsid = server.arg("ssid");

  if (LittleFS.exists("/wifi.json")) {
    File f = LittleFS.open("/wifi.json", FILE_READ);
    StaticJsonDocument<1024> oldDoc;
    StaticJsonDocument<1024> newDoc;
    JsonArray newNetworks = newDoc.to<JsonArray>();

    if (f) {
      if (!deserializeJson(oldDoc, f)) {
        JsonArray networks = oldDoc.as<JsonArray>();
        // Rebuild the array, skipping the one we want to delete
        for (JsonObject net : networks) {
          if (net["ssid"].as<String>() != targetSsid) {
            JsonObject newNet = newNetworks.createNestedObject();
            newNet["ssid"] = net["ssid"];
            newNet["pass"] = net["pass"];
          }
        }
      }
      f.close();
    }

    // Write the updated array back to flash
    File fWrite = LittleFS.open("/wifi.json", FILE_WRITE);
    serializeJson(newDoc, fWrite);
    fWrite.close();
  }

  // Redirect silently back to the WiFi page
  server.sendHeader("Location", "/wifi");
  server.send(303);
}

void handleSaveWifi() {
  String targetSsid = "";
  String targetPass = "";
  
  StaticJsonDocument<1024> doc;
  JsonArray networks;

  // Load existing saved networks
  if (LittleFS.exists("/wifi.json")) {
    File f = LittleFS.open("/wifi.json", FILE_READ);
    if (f) {
      deserializeJson(doc, f);
      networks = doc.as<JsonArray>();
      f.close();
    }
  } else {
    networks = doc.to<JsonArray>();
  }

  String selected = server.arg("saved_ssid");

  if (selected == "NEW" && server.hasArg("ssid") && server.hasArg("pass")) {
    // Save new network logic
    targetSsid = server.arg("ssid");
    targetPass = server.arg("pass");
    
    // Check if it already exists to avoid duplicates
    bool exists = false;
    for (JsonObject net : networks) {
      if (net["ssid"] == targetSsid) {
        net["pass"] = targetPass; // Update password
        exists = true;
        break;
      }
    }
    
    if (!exists) {
      JsonObject newNet = networks.createNestedObject();
      newNet["ssid"] = targetSsid;
      newNet["pass"] = targetPass;
    }

    // Save back to file
    File f = LittleFS.open("/wifi.json", FILE_WRITE);
    serializeJson(doc, f);
    f.close();

  } else {
    // Retrieve password for saved network
    targetSsid = selected;
    for (JsonObject net : networks) {
      if (net["ssid"] == targetSsid) {
        targetPass = net["pass"].as<String>();
        break;
      }
    }
  }

  if (targetSsid == "") {
    server.send(400, "text/plain", "Invalid Selection");
    return;
  }

  // Tell the browser we are starting
  server.send(200, "text/html", "<h3>Connecting to " + targetSsid + " and Syncing...</h3><p>Check the OLED screen for progress. You can return to the home page in 10 seconds.</p><a href='/'>Go Home</a>");

  // Switch to dual mode temporarily
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(targetSsid.c_str(), targetPass.c_str());

  showMessage("SYNCING...", "Connecting to:", targetSsid);
  
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Successfully connected to internet, run your sync function
    syncAllCSVs(); 
  } else {
    showMessage("SYNC FAILED", "Could not connect to", targetSsid);
    delay(3000);
  }

  // Drop the router connection and revert to pure AP mode
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  showMessage("READY! Scan Card", "AP IP: " + WiFi.softAPIP().toString());
}


bool uploadCSV(const String& path) {
  Serial.println("\n[UPLOAD] Starting CSV upload...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[UPLOAD]  WiFi not connected");
    showMessage("[UPLOAD]  WiFi not connected");
    delay(3000);
    return false;
  }

  Serial.print("[UPLOAD] WiFi OK. IP: ");
  Serial.println(WiFi.localIP());

  File file = LittleFS.open(path, FILE_READ);
  if (!file) {
    Serial.print("[UPLOAD]  Failed to open file: ");
    showMessage("[UPLOAD]  Failed to open file:", path);
    delay(3000);
    Serial.println(path);
    return false;
  }

  size_t size = file.size();
  Serial.print("[UPLOAD] File opened. Size: ");
  Serial.print(size);
  Serial.println(" bytes");

  if (size == 0) {
    Serial.println("[UPLOAD]  File is empty. Nothing to upload.");
    file.close();
    return false;
  }

  Serial.print("[UPLOAD] Server route: ");
  Serial.println(server_route);


  WiFiClientSecure client;
  client.setInsecure(); 

  HTTPClient http;

  if (!http.begin(client, server_route)) {
    Serial.println("[UPLOAD]  http.begin() failed");
    showMessage("[UPLOAD]  http.begin() failed");
    delay(3000);
    file.close();
    return false;
  }

  http.addHeader("Content-Type", "text/csv");

  Serial.println("[UPLOAD] Sending POST request...");
  int code = http.sendRequest("POST", &file, size);

  Serial.print("[UPLOAD] HTTP response code: ");
  Serial.println(code);

  if (code > 0) {
    Serial.println("[UPLOAD] Server response:");
    Serial.println(http.getString());
  } else {
    Serial.print("[UPLOAD]  HTTP error: ");
    Serial.println(http.errorToString(code));
  }

  http.end();
  file.close();

  return (code == 200);
}


bool isUploaded(const String& name) {
  if (!LittleFS.exists("/sync.json")) return false;

  File f = LittleFS.open("/sync.json", FILE_READ);
  StaticJsonDocument<512> doc;
  deserializeJson(doc, f);
  f.close();

  return doc[name] == true;
}

void markUploaded(const String& name) {
  StaticJsonDocument<512> doc;

  if (LittleFS.exists("/sync.json")) {
    File f = LittleFS.open("/sync.json", FILE_READ);
    deserializeJson(doc, f);
    f.close();
  }

  doc[name] = true;

  File f = LittleFS.open("/sync.json", FILE_WRITE);
  serializeJson(doc, f);
  f.close();
}

void markPending(const String& name) {
  StaticJsonDocument<512> doc;

  if (LittleFS.exists("/sync.json")) {
    File f = LittleFS.open("/sync.json", FILE_READ);
    deserializeJson(doc, f);
    f.close();
  }

  doc[name] = false;

  File f = LittleFS.open("/sync.json", FILE_WRITE);
  serializeJson(doc, f);
  f.close();
}


void wipeWifiAndRestart() {
  Serial.println("\n[SYSTEM] Sync complete! Deleting WiFi credentials...");
  
  if (LittleFS.exists("/wifi.json")) {
    LittleFS.remove("/wifi.json");
    Serial.println("[SYSTEM] WiFi credentials deleted.");
  }
  
  Serial.println("[SYSTEM] Rebooting into AP Mode...");
  delay(1000);
  ESP.restart();
}

void syncAllCSVs() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("\n[SYNC] Checking for files to upload...");

  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  bool anyError = false;  
  bool didWork = false;  

  while (file) {
    String rawName = String(file.name());
    String filePath;
    String cleanName;

    if (rawName.startsWith("/")) {
      filePath = rawName;
      cleanName = rawName.substring(1);
    } else {
      filePath = "/" + rawName;
      cleanName = rawName;
    }

    if (!file.isDirectory() && filePath.endsWith(".csv") && !filePath.equals(USERS_DB)) {
      
      if (!isUploaded(cleanName)) {
        didWork = true; 
        Serial.print("[SYNC] Found pending file: ");
        showMessage("[SYNC] Found pending file: ");
        Serial.println(filePath);

        if (uploadCSV(filePath)) {
          markUploaded(cleanName);
          Serial.println("[SYNC]  Upload success");
          showMessage("[SYNC]  Upload success");
        } else {
          Serial.println("[SYNC]  Upload failed");
          showMessage("[SYNC]  Upload failed");
          anyError = true; 
        }
      }
    }
    file = root.openNextFile();
  }

  if (!anyError) {
    Serial.println("[SYNC] All files synced successfully.");
    showMessage("[SYNC] Successfully synced All files.");
    delay(3000);
  } else {
    Serial.println("[SYNC] Some files failed to upload. Retrying next loop.");
    showMessage("[SYNC] Failed to upload some files.");
    delay(3000);
  }

  wipeWifiAndRestart();
}


String twoDigit(int x) {
  if (x < 10) return "0" + String(x);
  return String(x);
}

String getDate() {
  return twoDigit(now.day()) + "/" + twoDigit(now.month()) + "/" + String(now.year());
}

String getTime() {
  return twoDigit(now.hour()) + ":" + twoDigit(now.minute()) + ":" + twoDigit(now.second());
}

String uidToString(MFRC522::Uid *uid) {
  String s = "";
  for (byte i = 0; i < uid->size; i++) {
    if (uid->uidByte[i] < 0x10) s += "0";
    s += String(uid->uidByte[i], HEX);
    if (i != uid->size - 1) s += ":";
  }
  s.toUpperCase();
  return s;
}



void addUser(String uid, String name, String mID) {
  File file = LittleFS.open(USERS_DB, FILE_APPEND);
  if (!file) {
    Serial.println("FAILED TO OPEN DB FOR SAVING");
    return;
  }

  file.print(uid);
  file.print(",");
  file.print(name);
  file.print(",");
  file.println(mID);
  file.close();
}


String getSafeDateForFilename() {
  return String(now.year()) + "-" + twoDigit(now.month()) + "-" + twoDigit(now.day()) + "_Attendance.csv";
}

void processTap(const String &uid, String& name, String& mID, bool& isLogin) {
  String path = "/" + getSafeDateForFilename();
  String tempPath = "/temp_att.csv";
  markPending(getSafeDateForFilename());

  bool fileExisted = LittleFS.exists(path);
  isLogin = true; // Assume it's a login until we find an open session

  File tempFile = LittleFS.open(tempPath, FILE_WRITE);
  if (!tempFile) {
    Serial.println("Failed to open temp file.");
    return;
  }

  // Write header if the file is completely new today
  if (!fileExisted) {
    tempFile.println("MEMBER_ID,NAME,DATE,LOGIN_TIME,LOGOUT_TIME");
  } else {
    File file = LittleFS.open(path, FILE_READ);
    if (file) {
      while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        if (line.startsWith("MEMBER_ID")) {
          tempFile.println(line); // Copy header
          continue;
        }

        // Check if this line belongs to the user AND has an open session
        if (line.startsWith(mID + ",")) {
          int lastComma = line.lastIndexOf(',');
          String logoutVal = line.substring(lastComma + 1);
          
          if (logoutVal == "PENDING") {
            // Found an open login. This is a LOGOUT.
            isLogin = false;
            // Reconstruct the line, replacing "PENDING" with the current time
            String updatedLine = line.substring(0, lastComma + 1) + getTime();
            tempFile.println(updatedLine);
            continue; // Skip writing the original "PENDING" line
          }
        }
        // If not our target row, or it's a closed session, copy it exactly as is
        tempFile.println(line);
      }
      file.close();
    }
  }

  // If we scanned the whole file and didn't find a PENDING logout, it's a new LOGIN
  if (isLogin) {
    tempFile.print(mID);
    tempFile.print(",");
    tempFile.print(name);
    tempFile.print(",");
    tempFile.print(getDate());
    tempFile.print(",");
    tempFile.print(getTime());
    tempFile.println(",PENDING"); // Leave logout empty/pending
  }

  tempFile.close();

  // Swap the files
  if (fileExisted) {
    LittleFS.remove(path);
  }
  LittleFS.rename(tempPath, path);
}

void deleteUser(const String& uid) {
  if (!LittleFS.exists(USERS_DB)) return;

  File original = LittleFS.open(USERS_DB, FILE_READ);
  File temp = LittleFS.open("/temp_file.csv", FILE_WRITE);

  if (!original || !temp) return;

  while (original.available()){
    String line = original.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    if (!line.startsWith(uid + ",")){
      temp.println(line);
    }
  }

  original.close();
  temp.close();
  LittleFS.remove(USERS_DB);
  LittleFS.rename("/temp_file.csv", USERS_DB);
}

const char delete_page_header[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Delete Users</title>
<style>
  body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
  .card{max-width:720px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
  h2{margin:0 0 12px 0;color:#93c5fd}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;justify-content:space-between;padding:10px;border:1px solid #1f2933;border-radius:8px;margin:8px 0;background:#020617}
  .meta{font-size:13px;opacity:.85}
  form{margin:0}
  button{padding:8px 12px;border-radius:6px;border:1px solid #7f1d1d;background:#7f1d1d;color:#fff}
  button:hover{filter:brightness(1.1)}
  a{color:#93c5fd;text-decoration:none;display:inline-block;margin-top:10px}
</style>
</head>
<body>
  <div class="card">
    <h2>Delete Users</h2>
)rawliteral";

const char delete_page_footer[] PROGMEM = R"rawliteral(
    <a href="/">Back</a>
  </div>
</body>
</html>
)rawliteral";

void handleDeleteUserPage() {
 
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); 

  server.sendContent_P(delete_page_header);

  File file = LittleFS.open(USERS_DB, FILE_READ);
  if (!file) {
    server.sendContent("<p>No users found (Database missing).</p>");
  } else {
    bool usersFound = false;
    
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();

      if (line.length() == 0 || line == "UID,Name,MemberID") continue;

      int comma1 = line.indexOf(',');
      int comma2 = line.indexOf(',', comma1 + 1);

      if (comma1 > 0 && comma2 > 0) {
        usersFound = true;
        String uid = line.substring(0, comma1);
        String name = line.substring(comma1 + 1, comma2);
        String mID = line.substring(comma2 + 1);

        String rowHtml = "<div class='row'><div class='meta'><b>" + uid + "</b><br>";
        rowHtml += name + " | " + mID + "</div>";
        rowHtml += "<form action='/confirmDelete' method='POST'>";
        rowHtml += "<input type='hidden' name='uid' value='" + uid + "'>";
        rowHtml += "<button type='submit'>Delete</button></form></div>";

        server.sendContent(rowHtml);
      }
    }
    file.close();

    if (!usersFound) {
      server.sendContent("<p class='empty'>No users found.</p>");
    }
  }

  server.sendContent_P(delete_page_footer);
 
  server.sendContent(""); 
}

void handleApConfigPage() {
  const char html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AP Settings</title>
  <style>
    body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
    h2{margin:0 0 8px 0;color:#93c5fd}
    p{opacity:.8;font-size:14px;margin-bottom:16px}
    label{display:block;margin:12px 0 6px;font-size:14px;}
    input{width:100%;padding:12px;border-radius:8px;border:1px solid #1f2933;background:#020617;color:#e5e7eb;box-sizing:border-box;}
    input:focus{outline:none;border-color:#3b82f6;}
    button{margin-top:20px;width:100%;padding:12px;border-radius:8px;border:none;background:#2563eb;color:#fff;font-weight:600;cursor:pointer}
    button:hover{filter:brightness(1.05)}
    .warning{color:#f87171;font-size:12px;margin-top:8px;}
    a{color:#93c5fd;text-decoration:none;display:inline-block;margin-top:15px}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>Change AP Password</h2>
      <p>Change the password needed to connect directly to the ESP32's WiFi network.</p>
      <form action="/saveApConfig" method="POST">
        <label>New Password</label>
        <input type="password" name="ap_pass" placeholder="Minimum 8 characters" minlength="8" required>
        <div class="warning">Note: Changing this will disconnect you. You will need to reconnect with the new password.</div>
        <button type="submit">Save & Restart AP</button>
      </form>
      <a href="/">Back to Dashboard</a>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send_P(200, "text/html", html);
}

void handleSaveApConfig() {
  if (!server.hasArg("ap_pass")) {
    server.send(400, "text/plain", "Missing Password");
    return;
  }

  String newPass = server.arg("ap_pass");

  if (newPass.length() < 8) {
    server.send(400, "text/plain", "Password must be at least 8 characters!");
    return;
  }


  StaticJsonDocument<256> doc;
  doc["ap_pass"] = newPass;

  File f = LittleFS.open("/ap_config.json", FILE_WRITE);
  if (f) {
    serializeJson(doc, f);
    f.close();
  }

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Password Saved</title>
  <style>
    body{font-family:system-ui,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px;text-align:center;}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;border:1px solid #22c55e;}
    h2{color:#22c55e;margin-top:0;}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>Password Saved!</h2>
      <p>The ESP32 is restarting. Please reconnect to the WiFi network using your new password.</p>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
  
  
  delay(1000);
  ESP.restart();
}

void handleConfirmDelete() {
  if (!server.hasArg("uid")) {
    server.send(400, "text/plain", "Missing UID");
    return;
  }

  String uid = server.arg("uid");

  deleteUser(uid);

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>User Deleted</title>
  <style>
    body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35);text-align:center}
    h2{margin:0 0 8px 0;color:#f87171}
    p{opacity:.85;margin-bottom:16px}
    a{display:inline-block;text-decoration:none;color:#e5e7eb;background:#111827;padding:10px 14px;border-radius:8px;margin:6px;border:1px solid #1f2933}
    a.primary{background:#2563eb;border-color:#2563eb}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>User Deleted</h2>
      <p>The selected user has been removed.</p>
      <a class="primary" href="/deleteUser">Back to list</a>
      <a href="/">Home</a>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleRoot() {
  SaveMode = false;
  const char html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 RFID</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
    h2{margin:0 0 12px 0;color:#93c5fd}
    a{display:block;text-decoration:none;color:#e5e7eb;background:#111827;padding:12px;border-radius:8px;margin:8px 0;border:1px solid #1f2933}
    a:hover{background:#0b1220}
    .muted{opacity:.7;font-size:12px;margin-top:12px}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>ESP32 RFID</h2>
      <a href="/download">Download CSV</a>
      <a href="/wifi">Configure WiFi</a>
      <a href="/addUser">Add User</a>
      <a href="/deleteUser">Delete User</a>
      <a href="/rtc">Set Date & Time</a>
      <a href="/apConfig">Change AP Password</a>
      <div class="muted">AP mode dashboard</div>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send_P(200, "text/html", html);
}

void handleAddUser() {
  SaveMode = true;
  pendingUID = "";

  const char html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Add User</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
    h2{margin:0 0 8px 0;color:#93c5fd}
    p{opacity:.8}
    label{display:block;margin:12px 0 6px}
    input{width:100%;padding:12px;border-radius:8px;border:1px solid #1f2933;background:#020617;color:#e5e7eb}
    button{margin-top:12px;width:100%;padding:12px;border-radius:8px;border:none;background:#2563eb;color:#fff;font-weight:600}
    button:hover{filter:brightness(1.05)}
    .note{margin-top:12px;padding:10px;border-left:3px solid #2563eb;background:#020617}
    a{color:#93c5fd;text-decoration:none;display:inline-block;margin-top:10px}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>Add User</h2>
      <p>1) Scan RFID card<br>2) Enter details and submit</p>
      <form action="/saveUser" method="POST">
        <label>Name</label>
        <input type="text" name="name" required>
        <label>Member ID</label>
        <input type="text" name="mID" required>
        <button type="submit">Save User</button>
      </form>
      <div class="note">Scan the card before submitting</div>
      <a href="/">Back</a>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send_P(200, "text/html", html);
}


void handleSaveUser() {
  auto sendPage = [&](const String& title, const String& msg, bool success) {
    String color = success ? "#22c55e" : "#f87171";
    String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>)rawliteral" + title + R"rawliteral(</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35);text-align:center}
    h2{margin:0 0 8px 0;color:)rawliteral" + color + R"rawliteral(}
    p{opacity:.85;margin-bottom:16px}
    a{display:inline-block;text-decoration:none;color:#e5e7eb;background:#111827;padding:10px 14px;border-radius:8px;margin:6px;border:1px solid #1f2933}
    a.primary{background:#2563eb;border-color:#2563eb}
    a:hover{filter:brightness(1.05)}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>)rawliteral" + title + R"rawliteral(</h2>
      <p>)rawliteral" + msg + R"rawliteral(</p>
      <a class="primary" href="/">Home</a>
      <a href="/addUser">Add Another</a>
    </div>
  </body>
  </html>
  )rawliteral";

      server.send(success ? 200 : 400, "text/html", html);
  };

  if (!server.hasArg("name") || !server.hasArg("mID")) {
    sendPage("Missing Fields", "Please fill in all required fields.", false);
    return;
  }

  if (pendingUID == "") {
    sendPage("No Card Scanned", "Scan the RFID card before submitting the form.", false);
    return;
  }
  String name, mID;
  if (findUsers(pendingUID, name, mID)) {
    sendPage("Duplicate UID", "This card is already registered.", false);
    return;
  }

  addUser(pendingUID, server.arg("name"), server.arg("mID"));
  SaveMode = false;
  pendingUID = "";

  sendPage("User Added", "The user was added successfully.", true);
}

const char download_page_header[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Download CSV</title>
<style>
  body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
  .card{max-width:720px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
  h2{color:#93c5fd;margin-bottom:12px}
  .row{display:flex;justify-content:space-between;align-items:center;padding:10px;border:1px solid #1f2933;border-radius:8px;margin:8px 0}
  .left{display:flex;flex-direction:column}
  .name{font-weight:600}
  .status{font-size:12px;color:#94a3b8}
  a.btn{background:#2563eb;color:#fff;padding:6px 10px;border-radius:6px;text-decoration:none;margin-left:6px}
  a.del{background:#7f1d1d}
</style>
</head>
<body>
<div class="card">
<h2>Attendance CSV Files</h2>
)rawliteral";

void handleRTCPage() {
  const char html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Set Date & Time</title>
  <style>
    body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
    h2{margin:0 0 8px 0;color:#93c5fd}
    p{opacity:.8}
    label{display:block;margin:12px 0 6px}
    input{width:100%;padding:12px;border-radius:8px;border:1px solid #1f2933;background:#020617;color:#e5e7eb;box-sizing:border-box;}
    input[type="date"]::-webkit-calendar-picker-indicator, input[type="time"]::-webkit-calendar-picker-indicator { filter: invert(1); }
    button{margin-top:20px;width:100%;padding:12px;border-radius:8px;border:none;background:#2563eb;color:#fff;font-weight:600;cursor:pointer}
    button:hover{filter:brightness(1.05)}
    a{color:#93c5fd;text-decoration:none;display:inline-block;margin-top:15px}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>Set Date & Time</h2>
      <p>Adjust the internal RTC module clock.</p>
      <form action="/saveRTC" method="POST">
        <label>Date</label>
        <input type="date" name="date" required>
        <label>Time</label>
        <input type="time" name="time" required>
        <button type="submit">Update RTC</button>
      </form>
      <a href="/">Back to Dashboard</a>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send_P(200, "text/html", html);
}

void handleSaveRTC() {
  if (!server.hasArg("date") || !server.hasArg("time")) {
    server.send(400, "text/plain", "Missing Date or Time");
    return;
  }

  String dateStr = server.arg("date");
  String timeStr = server.arg("time"); 


  int year = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();


  int hour = timeStr.substring(0, 2).toInt();
  int minute = timeStr.substring(3, 5).toInt();


  rtc.adjust(DateTime(year, month, day, hour, minute, 0));
  
  now = rtc.now();


  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>RTC Updated</title>
  <style>
    body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e5e7eb;margin:0;padding:16px;text-align:center;}
    .card{max-width:520px;margin:auto;background:#020617;border-radius:12px;padding:20px;border:1px solid #22c55e;}
    h2{color:#22c55e;margin-top:0;}
    a{display:inline-block;padding:10px 16px;background:#2563eb;color:#fff;text-decoration:none;border-radius:8px;margin-top:15px;}
  </style>
  </head>
  <body>
    <div class="card">
      <h2>Time Updated Successfully!</h2>
      <p>The hardware clock has been synchronized.</p>
      <a href="/">Back to Dashboard</a>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
  
  showMessage("RTC Updated!", getDate(), getTime());
  delay(2000); 
  showMessage("READY! Scan Card", "AP IP: " + WiFi.softAPIP().toString());
}

void handleDownload() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  
  server.sendContent_P(download_page_header);

  File root = LittleFS.open("/");
  File file = root.openNextFile();
  bool found = false;

  while (file) {
    String rawName = String(file.name());
    String path; 
    String cleanName;

    if (rawName.startsWith("/")) {
      path = rawName;
      cleanName = rawName.substring(1);
    } else {
      path = "/" + rawName;
      cleanName = rawName;
    }
    
    if (!file.isDirectory() && path.indexOf("Attendance") >= 0 && path.endsWith(".csv")) {
      bool uploaded = isUploaded(cleanName);
      String status = uploaded ? "Uploaded" : "Pending";
      found = true;

      String rowHtml = "<div class='row'><div class='left'><div class='name'>" + cleanName + "</div>";
      rowHtml += "<div class='status'>" + status + "</div></div><div>";
      rowHtml += "<a class='btn' href='/get?file=" + cleanName + "'>Download</a>";
      rowHtml += "<a class='btn del' href='/deleteCsv?file=" + cleanName + "'>Delete</a>";
      rowHtml += "</div></div>";
      
      server.sendContent(rowHtml); // Stream immediately
    }
    file = root.openNextFile();
  }

  if (!found) {
    server.sendContent("<p>No attendance files found.</p>");
  }

  server.sendContent("<a href='/'>Back</a></div></body></html>");
  server.sendContent(""); 
}

void handleGetCSV() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }

  String path = "/" + server.arg("file");

  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  File file = LittleFS.open(path, FILE_READ);
  server.streamFile(file, "text/csv");
  file.close();
}

void handleDeleteCSV() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing file");
    return;
  }

  String path = "/" + server.arg("file");

  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  LittleFS.remove(path);
  server.sendHeader("Location", "/download");
  server.send(303);
}

bool findUsers(const String& targetUid, String& outName, String& outMID){
  if (!LittleFS.exists(USERS_DB)) return false;
  File file = LittleFS.open(USERS_DB, FILE_READ);
  if (!file) return false;

  bool found = false;
  while (file.available()){
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    int first = line.indexOf(',');
    int second = line.indexOf(',', first + 1);

    if (first > 0 && second > 0){
      String uid = line.substring(0, first);
      if (uid == targetUid){
        found = true;
        outName = line.substring(first+1, second);
        outMID = line.substring(second+1);
        break;
      }
    }
  }
  file.close();
  return found;
}

void verifyUID(String uid) {
  String name, mID;
  if (findUsers(uid, name, mID)) {
    bool isLogin = true;
    
    // Process the tap and determine the state directly from the file rewrite
    processTap(uid, name, mID, isLogin);
    
    // Update the OLED based on the state returned
    if (isLogin) {
      showMessage("--- LOGGED IN ---", name, getTime());
    } else {
      showMessage("--- LOGGED OUT ---", name, getTime());
    }
    
    digitalWrite(GREEN_LED_PIN, HIGH);
    tone(BUZZER_PIN, 2000, 120);
    delay(1500); // Give user time to read the screen
    digitalWrite(GREEN_LED_PIN, LOW);
    
    // Reset screen
    showMessage("READY! Scan Card", "AP IP: " + WiFi.softAPIP().toString());

  } else {
    showMessage("Access Denied", uid);
    digitalWrite(RED_LED_PIN, HIGH);
    tone(BUZZER_PIN, 400, 200);
    delay(1500);
    digitalWrite(RED_LED_PIN, LOW);
    showMessage("READY! Scan Card", "AP IP: " + WiFi.softAPIP().toString());
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  showMessage("Booting...");

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init();

  if (!rtc.begin()){
    Serial.println("Could not connect with RTC.");
    showMessage("Could not connect with RTC");
    while (1);
  }

  if (rtc.lostPower()){
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    showMessage("RTC reset. Set Date and Time in AP mode.");
    delay(2000);
  }
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  if (!LittleFS.begin(false)) {
    Serial.println("LittleFS failed");
    showMessage("LittleFS failed");
    while (1);
  }

  if (!LittleFS.exists(USERS_DB)) {
    Serial.println("Creating new User Database...");
    File f = LittleFS.open(USERS_DB, FILE_WRITE);
    f.println("UID,Name,MemberID"); 
    f.close();
  }

  startAP();

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/download", handleDownload);
  server.on("/addUser", handleAddUser);
  server.on("/saveUser", HTTP_POST, handleSaveUser);
  server.on("/deleteUser", handleDeleteUserPage);
  server.on("/confirmDelete", HTTP_POST, handleConfirmDelete);
  server.on("/get", handleGetCSV);
  server.on("/deleteCsv", handleDeleteCSV);
  server.on("/wifi", handleWifiPage);
  server.on("/saveWifi", HTTP_POST, handleSaveWifi);
  server.on("/rtc", handleRTCPage);
  server.on("/saveRTC", HTTP_POST, handleSaveRTC);
  server.on("/apConfig", handleApConfigPage);
  server.on("/saveApConfig", HTTP_POST, handleSaveApConfig);
  server.on("/deleteWifi", HTTP_POST, handleDeleteWifi);

  server.begin();
  showMessage("READY! Scan Card", "AP IP: "+ WiFi.softAPIP().toString());
}

void loop() {
  now = rtc.now();
  server.handleClient();
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  
 
  lastScannedUID = uidToString(&mfrc522.uid);
  Serial.println(lastScannedUID);

  

  if (!SaveMode) {
    verifyUID(lastScannedUID);
  } else {
    pendingUID = lastScannedUID;
    showMessage("Scanned UID:", pendingUID);
    digitalWrite(GREEN_LED_PIN, HIGH);
    tone(BUZZER_PIN, 1200, 80);
    delay(200);
    digitalWrite(GREEN_LED_PIN, LOW);
  }

  mfrc522.PICC_HaltA();
  delay(800);
}
