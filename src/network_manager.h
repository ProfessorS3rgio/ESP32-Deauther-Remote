#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"

class NetworkManager {
public:
    static void begin();
    static bool connectWiFi(const char* ssid, const char* pass, int timeoutSec);
    static bool connectMQTT(PubSubClient& mqtt, WiFiClientSecure& wifiClient, 
                           void (*callback)(char*, byte*, unsigned int));
    static void disconnect();
    static void reconnectMQTT(PubSubClient& mqtt, WiFiClientSecure& wifiClient,
                             void (*callback)(char*, byte*, unsigned int));
    static bool isWiFiConnected();
    static bool isMQTTConnected(PubSubClient& mqtt);
    
private:
    static const int MAX_WIFI_RETRIES = 3;
    static const int MAX_MQTT_RETRIES = 3;
};