#pragma once

#include <Arduino.h>
#include <vector>


// Add to TargetAP struct:
struct TargetAP {
    uint8_t bssid[6];
    int channel;
    String ssid;
    bool active;
    int consecutive_bursts;
    unsigned long last_attack;
    unsigned long next_attack;
    int clients_detected;
    int clients_before_attack;   // ✅ NEW
    int clients_after_attack;    // ✅ NEW
    int total_disconnected;      // ✅ NEW
};

// Add new functions:
int count_clients_on_ap(uint8_t* bssid);
void report_attack_results(TargetAP& target);

struct VendoConfig {
    String unique_name;      // e.g., "Razor"
    uint8_t target_bssid[6]; // Primary target BSSID
    bool bssid_locked;       // Lock onto BSSID even if SSID changes
    int channel;
    bool auto_discover;      // Auto-discover based on SSID patterns
};

void start_deauth(int wifi_number, int attack_type, uint16_t reason);
void start_multi_deauth(std::vector<int> wifi_numbers, uint16_t reason);
void start_vendo_ghost(VendoConfig config);
void stop_deauth();
void update_multi_attack();
void update_vendo_ghost();  // Main stealth loop
int get_current_multi_channel();
bool is_multi_attack_running();
bool is_vendo_ghost_running();
void set_vendo_unique_name(String name);
void scan_for_vendo_targets();
void check_for_clients();
bool has_clients_connected();

extern int eliminated_stations;
extern int deauth_type;
extern std::vector<TargetAP> multi_targets;
extern int current_multi_target;
extern VendoConfig vendo_config;
extern bool vendo_ghost_active;

