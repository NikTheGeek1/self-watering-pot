#pragma once

#include <Preferences.h>

#include <Arduino.h>

#include "watering_history.h"

struct PlantSettings {
  int dryRaw = -1;
  int wetRaw = -1;
  uint8_t dryThresholdPercent = 35;
  uint32_t pumpPulseMs = 1200;
  uint32_t cooldownMs = 45000;
  uint32_t sampleIntervalMs = 5000;
  bool autoEnabled = false;
};

struct WiFiCredentials {
  String ssid;
  String password;

  bool isConfigured() const { return !ssid.isEmpty(); }
};

class ConfigStore {
 public:
  bool begin();

  PlantSettings loadPlantSettings();
  void savePlantSettings(const PlantSettings& settings);

  WiFiCredentials loadWiFiCredentials();
  void saveWiFiCredentials(const WiFiCredentials& credentials);
  void clearWiFiCredentials();

  size_t loadWateringHistory(WateringEvent* events, size_t capacity);
  void saveWateringHistory(const WateringEvent* events, size_t count);

 private:
  Preferences preferences_;
  bool begun_ = false;
};
