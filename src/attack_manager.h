#pragma once

#include <WiFi.h>
#include <vector>
#include "remote_control_types.h"

class AttackManager {
public:
    static void begin();
    static void startAttack(const std::vector<AttackTarget>& targets, uint16_t reason);
    static void resumeAttack(const std::vector<AttackTarget>& savedTargets, 
                            uint16_t reason, bool& attackModeActive);
    static void runAttackLoop(bool attackModeActive, bool attackPaused);
    static void scanAndReport(std::vector<ScannedNetwork>& results);
    
private:
    static float calculateDistance(int rssi);
    static bool matchesKeywords(const String& ssid);
};