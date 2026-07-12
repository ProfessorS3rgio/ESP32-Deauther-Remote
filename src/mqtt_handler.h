#pragma once

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"
#include "remote_control_types.h"

class MQTTHandler {
public:
    static void begin();
    static void callback(char* topic, byte* payload, unsigned int length, 
                        PubSubClient& mqtt, bool& attackTriggered, 
                        bool& attackModeActive, bool& attackPaused,
                        std::vector<AttackTarget>& attackTargets,
                        std::vector<AttackTarget>& savedAttackTargets,
                        uint16_t& savedReason, unsigned long& checkInterval,
                        unsigned long& lastCheckTime, uint16_t& lastReason);
    static String sanitizeSsid(const String& ssid);
    static void publishStatus(PubSubClient& mqtt, const char* status, 
                             const char* mode = "", bool paused = false);
    static void publishScanResults(PubSubClient& mqtt, 
                                  const std::vector<ScannedNetwork>& networks);
    
private:
    // Command handlers
    static void handleStartAttack(PubSubClient& mqtt, bool& attackTriggered);
    static void handleStopAttack(PubSubClient& mqtt, bool& attackTriggered,
                                bool& attackModeActive, bool& attackPaused);
    static void handleStatus(PubSubClient& mqtt, bool attackModeActive,
                            std::vector<AttackTarget>& attackTargets);
    static void handleScan(PubSubClient& mqtt);
    static void handleMultiAttack(PubSubClient& mqtt, const String& msg,
                                 std::vector<AttackTarget>& attackTargets,
                                 std::vector<AttackTarget>& savedAttackTargets,
                                 uint16_t& savedReason, unsigned long& lastCheckTime,
                                 bool& attackModeActive, bool& attackTriggered,
                                 uint16_t& lastReason);
    static void handleRestart(PubSubClient& mqtt);
    static void handleSetInterval(const String& msg, PubSubClient& mqtt, 
                                 unsigned long& checkInterval);
    static void handleOTAUpdate(PubSubClient& mqtt, const String& url);
    
    // Helpers
    static void parseTargetString(const String& target, 
                                 std::vector<AttackTarget>& attackTargets);
};