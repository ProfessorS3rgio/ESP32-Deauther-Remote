#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <vector>
#include "remote_control_types.h"

class RemoteControl {
public:
    static void begin();
    static void loop();
    static bool shouldAttack() { return attackTriggered; }
    static bool isAttackActive() { return attackModeActive; }
    static void runAttackLoop();
    
    // Public for MQTT callback access
    static bool attackTriggered;
    static bool attackModeActive;
    static bool attackPaused;
    static std::vector<AttackTarget> attackTargets;
    static std::vector<AttackTarget> savedAttackTargets;
    static uint16_t savedReason;
    static uint16_t lastReason;
    static unsigned long checkInterval;
    static unsigned long lastCheckTime;
    static unsigned long lastStatusTime;
    static WiFiClientSecure wifiClient;
    static PubSubClient mqtt;
    
private:
    static std::vector<ScannedNetwork> scanResults;
    static void performMQTTCheckIn();
    static void handleStandbyMode(unsigned long currentTime);
    static void handlePausedMode();
    static void sendStatus();
};