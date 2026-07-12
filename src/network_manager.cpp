#include "network_manager.h"

void NetworkManager::begin() {
    Serial.println("📶 Network Manager initialized");
}

bool NetworkManager::connectWiFi(const char* ssid, const char* pass, int timeoutSec) {
    const int maxRetries = 3;
    
    for (int retry = 1; retry <= maxRetries; retry++) {
        if (retry > 1) {
            Serial.printf("\n🔄 WiFi Retry %d/%d\n", retry, maxRetries);
            delay(3000);
        }
        
        Serial.printf("📶 Connecting to WiFi: %s\n", ssid);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        delay(200);
        WiFi.begin(ssid, pass);
        
        int attempts = timeoutSec * 2;
        for (int i = 0; i < attempts; i++) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("\n✅ WiFi Connected!\n");
                Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("   RSSI: %d dBm\n", WiFi.RSSI());
                return true;
            }
            delay(500);
            Serial.print(".");
            if ((i + 1) % 20 == 0) Serial.printf(" %ds\n", (i + 1) / 2);
        }
        
        Serial.printf("\n❌ WiFi attempt %d failed!\n", retry);
    }
    
    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║     ❌ WIFI CONNECTION FAILED       ║");
    Serial.println("║     ALL ATTEMPTS EXHAUSTED          ║");
    Serial.println("╚══════════════════════════════════════╝");
    Serial.printf("   SSID: %s\n", ssid);
    Serial.printf("   Attempts: %d\n", maxRetries);
    
    Serial.println("\n🔄 RESTARTING DEVICE IN 5 SECONDS...");
    for (int i = 5; i > 0; i--) {
        Serial.printf("   %d...\n", i);
        delay(1000);
    }
    Serial.println("💀 Restarting now!\n");
    ESP.restart();
    
    return false;
}

bool NetworkManager::connectMQTT(PubSubClient& mqtt, WiFiClientSecure& wifiClient,
                                 void (*callback)(char*, byte*, unsigned int)) {
    if (!isWiFiConnected()) return false;
    
    Serial.println("☁️  Setting up MQTT...");
    wifiClient.setInsecure();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(callback);
    mqtt.setBufferSize(4096);
    
    reconnectMQTT(mqtt, wifiClient, callback);
    return mqtt.connected();
}

void NetworkManager::reconnectMQTT(PubSubClient& mqtt, WiFiClientSecure& wifiClient,
                                   void (*callback)(char*, byte*, unsigned int)) {
    if (!isWiFiConnected()) {
        Serial.println("❌ Cannot connect MQTT - WiFi not connected");
        return;
    }
    
    wifiClient.setInsecure();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(callback);
    mqtt.setBufferSize(4096);
    
    while (!mqtt.connected()) {
        Serial.print("🔄 Connecting to HiveMQ...");
        String clientId = String(DEVICE_ID) + "_" + String(random(0xffff), HEX);
        
        if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println(" ✅");
            
            // Subscribe to device-specific topics
            mqtt.subscribe(TOPIC_CMD);
            mqtt.subscribe(TOPIC_CMD_RETAINED);
            
            // ✅ Subscribe to broadcast topic (all devices)
            mqtt.subscribe(TOPIC_BROADCAST);
            
            // Clear stale retained messages
            mqtt.publish(TOPIC_CMD_RETAINED, "", true);
            mqtt.loop();
            delay(100);
            
            // Publish online status with device ID
            String onlineMsg = "{\"status\":\"online\",\"device\":\"" + String(DEVICE_ID) + "\"}";
            mqtt.publish(TOPIC_STATUS, onlineMsg.c_str());
            
            Serial.printf("   Device ID: %s\n", DEVICE_ID);
            Serial.printf("   Subscribed: %s, %s, %s\n", TOPIC_CMD, TOPIC_CMD_RETAINED, TOPIC_BROADCAST);
        } else {
            Serial.printf(" ❌ (rc=%d). Retry in 5s...\n", mqtt.state());
            delay(5000);
        }
    }
}

void NetworkManager::disconnect() {
    Serial.println("🔌 Disconnecting from network...");
    WiFi.disconnect(true);
    delay(100);
}

bool NetworkManager::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::isMQTTConnected(PubSubClient& mqtt) {
    return mqtt.connected();
}