#pragma once

#include "secrets.h"  // Your private credentials (gitignored)

// ============================================
// MQTT BROKER (Public)
// ============================================
#define MQTT_BROKER "17bd4d19c8574ecca41de5d41d95e99a.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883

// ============================================
// DEVICE IDENTIFICATION
// ============================================
// Change for each ESP32: 01, 02, 03...
// #define DEVICE_ID "01"
#define DEVICE_ID "02"


// ============================================
// DEVICE & TOPICS
// ============================================
#define TOPIC_CMD         "vendoghost/" DEVICE_ID "/command"
#define TOPIC_CMD_RETAINED "vendoghost/" DEVICE_ID "/command/retained"
#define TOPIC_STATUS      "vendoghost/" DEVICE_ID "/status"
#define TOPIC_SCAN        "vendoghost/" DEVICE_ID "/scan"
#define TOPIC_BROADCAST   "vendoghost/all/command"

// ============================================
// SCANNING
// ============================================
#define SCAN_KEYWORD_COUNT 9
extern const char* SCAN_KEYWORDS[SCAN_KEYWORD_COUNT];

// ============================================
// TIMING
// ============================================
#define STATUS_INTERVAL 30000        // 30 seconds
#define DEFAULT_CHECK_INTERVAL 60000 // 1 minute
// #define DEFAULT_CHECK_INTERVAL 120000 // 2 minutes
#define MQTT_LISTEN_TIME 10000       // 10 seconds
#define WIFI_TIMEOUT 15              // 15 seconds
#define WIFI_CHECK_TIMEOUT 10        // 10 seconds