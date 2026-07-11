#pragma once

// ============================================
// WIFI CONFIGURATION
// ============================================
// #define WIFI_SSID "6621"
// #define WIFI_PASS "wlan121c"


#define WIFI_SSID "Printer"
#define WIFI_PASS "@20SuperMan02@1"
// ============================================
// MQTT CONFIGURATION (HiveMQ Cloud)
// ============================================
#define MQTT_BROKER "17bd4d19c8574ecca41de5d41d95e99a.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "ProfessorS3rgio"
#define MQTT_PASS "@20SuperMan02@"

// ============================================
// DEVICE & TOPICS
// ============================================
#define DEVICE_ID "vendoghost_001"
#define TOPIC_CMD "vendoghost/command"
#define TOPIC_CMD_RETAINED "vendoghost/command/retained"
#define TOPIC_STATUS "vendoghost/status"
#define TOPIC_SCAN "vendoghost/scan"

// ============================================
// SCANNING
// ============================================
#define SCAN_KEYWORD_COUNT 9
extern const char* SCAN_KEYWORDS[SCAN_KEYWORD_COUNT];

// ============================================
// TIMING
// ============================================
#define STATUS_INTERVAL 30000        // 30 seconds
// #define DEFAULT_CHECK_INTERVAL 120000 // 2 minutes
#define DEFAULT_CHECK_INTERVAL 60000 // 1 minute
#define MQTT_LISTEN_TIME 10000       // 10 seconds
#define WIFI_TIMEOUT 15              // 15 seconds
#define WIFI_CHECK_TIMEOUT 10        // 10 seconds