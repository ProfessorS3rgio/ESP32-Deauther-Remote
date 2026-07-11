#pragma once

// your led pin (if you have one)
//#define LED 2

//enable serial debug (if needed)
// #define SERIAL_DEBUG

#define CHANNEL_MAX 13
#define NUM_FRAMES_PER_DEAUTH 3  // Reduced for stealth (was 16)
#define DEAUTH_BLINK_TIMES 1
#define DEAUTH_BLINK_DURATION 10
#define DEAUTH_TYPE_SINGLE 0
#define DEAUTH_TYPE_ALL 1
#define DEAUTH_TYPE_MULTI 2
#define DEAUTH_TYPE_VENDO 3  // New stealth mode

// Stealth attack parameters
#define MAX_MULTI_TARGETS 5
#define CHANNEL_HOP_INTERVAL 100
#define STEALTH_MIN_DELAY 30000   // 30 seconds minimum between bursts
#define STEALTH_MAX_DELAY 120000  // 2 minutes maximum between bursts
#define STEALTH_BURST_COUNT 2     // Number of deauth frames per burst
#define STEALTH_SCAN_INTERVAL 30000 // Check for clients every 30 seconds
#define STEALTH_MAX_CONSECUTIVE 3  // Max consecutive bursts before longer pause
#define STEALTH_LONG_PAUSE_MIN 180000 // 3 minutes minimum long pause
#define STEALTH_LONG_PAUSE_MAX 600000 // 10 minutes maximum long pause

// Vendo-specific keywords
#define VENDO_KEYWORDS_COUNT 4
extern const char* VENDO_KEYWORDS[];


#ifdef SERIAL_DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#endif
#ifndef SERIAL_DEBUG
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTF(...)
#endif
#ifdef LED
#define BLINK_LED(num_times, blink_duration) blink_led(num_times, blink_duration)
#endif
#ifndef LED
#define BLINK_LED(...)
#endif

void blink_led(int num_times, int blink_duration);