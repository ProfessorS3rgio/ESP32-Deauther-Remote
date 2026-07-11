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
    
    // All retries failed - restart device
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
    
    return false; // Never reached
}

bool NetworkManager::connectMQTT(PubSubClient& mqtt, WiFiClientSecure& wifiClient,
                                 void (*callback)(char*, byte*, unsigned int)) {
    if (!isWiFiConnected()) return false;
    
    Serial.println("☁️  Setting up MQTT...");
    wifiClient.setInsecure();
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(callback);
    mqtt.setBufferSize(2048);
    
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
    mqtt.setBufferSize(2048);
    
    while (!mqtt.connected()) {
        Serial.print("🔄 Connecting to HiveMQ...");
        String clientId = DEVICE_ID + String(random(0xffff), HEX);
        
        if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println(" ✅");
            mqtt.subscribe(TOPIC_CMD);
            mqtt.subscribe(TOPIC_CMD_RETAINED);
            
            mqtt.publish(TOPIC_CMD_RETAINED, "", true);
            mqtt.loop();
            delay(100);
            
            mqtt.publish(TOPIC_STATUS, "{\"status\":\"online\"}");
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