# 👻 Vendo Ghost - ESP32 MQTT Deauther

ESP32-based WiFi deauthentication tool controlled remotely via MQTT. Target and disrupt PisoWiFi and other 2.4GHz networks using IEEE 802.11 deauth frames. Fully remote operation with MQTT command interface and OTA update support.

## ⚠️ DISCLAIMER

**This project is made for learning and educational purposes only. I do not accept any responsibility for any trouble, damage, or illegal activities that may result from the use of this software. Users are solely responsible for ensuring their use complies with local laws and regulations.**

## ✨ Features

- 🔴 **Remote MQTT Control** - No physical access needed
- 📡 **Network Scanning** - Auto-detects PisoWiFi targets
- ⚔️ **Multi-Target Attack** - Deauth multiple networks simultaneously
- 📱 **Flutter App** - Android control interface
- 🔄 **OTA Updates** - Update firmware remotely via GitHub Releases
- 📊 **Real-time Status** - Monitor via MQTT topics
- 🔁 **Auto-Recovery** - Periodic MQTT check-ins during attack
- 🎯 **Smart Targeting** - Keyword-based PisoWiFi detection

## 🏗️ Architecture


┌─────────────┐ MQTT ┌──────────────┐ WiFi Deauth ┌─────────────┐
│ Flutter App │◄──────────►│ ESP32 Vendo │◄────────────────►│ Target WiFi │
│ (Android) │ HiveMQ │ Ghost Device │ 802.11 Frames │ Networks │
└─────────────┘ Cloud └──────────────┘ └─────────────┘


## 📋 Requirements

### Hardware
- **ESP32 Development Board** (ESP32-WROOM-32, DevKit v1, etc.)
- USB Cable for programming

### Software
- **VSCode** with PlatformIO extension
- **Flutter** (for Android app - optional)
- **MQTT Client** (HiveMQ Web Client, MQTT Explorer, or Flutter app)

### Services
- **HiveMQ Cloud** (Free MQTT broker)
- **GitHub** (For OTA firmware hosting)

## 🚀 Installation

### 1. Clone the repository
```bash
git clone https://github.com/ProfessorS3rgio/ESP32-Deauther-Remote.git
cd ESP32-Deauther-Remote