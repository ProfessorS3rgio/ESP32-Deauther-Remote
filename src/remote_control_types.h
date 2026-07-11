#pragma once

#include <Arduino.h>
#include <vector>

struct ScannedNetwork {
    String ssid;
    String bssid;
    int channel;
    int rssi;
    float distance;
    bool is_target;
};

struct AttackTarget {
    String ssid;
    uint8_t bssid[6];
    int channel;
};