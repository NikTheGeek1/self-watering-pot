#include "config_store.h"

#include "app_constants.h"

namespace {

uint32_t clampUint32(uint32_t value, uint32_t minimumValue, uint32_t maximumValue) {
  if (value < minimumValue) {
    return minimumValue;
  }

  if (value > maximumValue) {
    return maximumValue;
  }

  return value;
}

String wateringHistoryKey(size_t index, const char* suffix) {
  String key = "wh";
  key += index;
  key += "_";
  key += suffix;
  return key;
}

void removeWateringEvent(Preferences& preferences, size_t index) {
  const String sequenceKey = wateringHistoryKey(index, "seq");
  const String reasonKey = wateringHistoryKey(index, "why");
  const String startedAtKey = wateringHistoryKey(index, "st");
  const String endedAtKey = wateringHistoryKey(index, "et");
  const String durationKey = wateringHistoryKey(index, "dur");
  const String startRawKey = wateringHistoryKey(index, "sr");
  const String startPercentKey = wateringHistoryKey(index, "sp");
  const String endRawKey = wateringHistoryKey(index, "er");
  const String endPercentKey = wateringHistoryKey(index, "ep");

  if (preferences.isKey(sequenceKey.c_str())) {
    preferences.remove(sequenceKey.c_str());
  }
  if (preferences.isKey(reasonKey.c_str())) {
    preferences.remove(reasonKey.c_str());
  }
  if (preferences.isKey(startedAtKey.c_str())) {
    preferences.remove(startedAtKey.c_str());
  }
  if (preferences.isKey(endedAtKey.c_str())) {
    preferences.remove(endedAtKey.c_str());
  }
  if (preferences.isKey(durationKey.c_str())) {
    preferences.remove(durationKey.c_str());
  }
  if (preferences.isKey(startRawKey.c_str())) {
    preferences.remove(startRawKey.c_str());
  }
  if (preferences.isKey(startPercentKey.c_str())) {
    preferences.remove(startPercentKey.c_str());
  }
  if (preferences.isKey(endRawKey.c_str())) {
    preferences.remove(endRawKey.c_str());
  }
  if (preferences.isKey(endPercentKey.c_str())) {
    preferences.remove(endPercentKey.c_str());
  }
}

}  // namespace

bool ConfigStore::begin() {
  if (begun_) {
    return true;
  }

  begun_ = preferences_.begin(kPreferencesNamespace, false);
  return begun_;
}

PlantSettings ConfigStore::loadPlantSettings() {
  PlantSettings settings;

  settings.dryRaw = preferences_.getInt("dry_raw", -1);
  settings.wetRaw = preferences_.getInt("wet_raw", -1);
  settings.dryThresholdPercent = static_cast<uint8_t>(
      clampUint32(preferences_.getUChar("threshold", kDefaultDryThresholdPercent),
                  kMinThresholdPercent, kMaxThresholdPercent));
  settings.pumpPulseMs =
      clampUint32(preferences_.getUInt("pulse_ms", kDefaultPumpPulseMs), kMinPumpPulseMs,
                  kMaxPumpPulseMs);
  settings.cooldownMs =
      clampUint32(preferences_.getUInt("cool_ms", kDefaultCooldownMs), kMinCooldownMs,
                  kMaxCooldownMs);
  settings.sampleIntervalMs =
      clampUint32(preferences_.getUInt("sample_ms", kDefaultSampleIntervalMs),
                  kMinSampleIntervalMs, kMaxSampleIntervalMs);

  // Safe default: automatic watering always starts disabled on boot.
  settings.autoEnabled = false;
  return settings;
}

void ConfigStore::savePlantSettings(const PlantSettings& settings) {
  preferences_.putInt("dry_raw", settings.dryRaw);
  preferences_.putInt("wet_raw", settings.wetRaw);
  preferences_.putUChar("threshold", settings.dryThresholdPercent);
  preferences_.putUInt("pulse_ms", settings.pumpPulseMs);
  preferences_.putUInt("cool_ms", settings.cooldownMs);
  preferences_.putUInt("sample_ms", settings.sampleIntervalMs);
}

WiFiCredentials ConfigStore::loadWiFiCredentials() {
  WiFiCredentials credentials;
  credentials.ssid = preferences_.getString("wifi_ssid", "");
  credentials.password = preferences_.getString("wifi_pass", "");
  return credentials;
}

void ConfigStore::saveWiFiCredentials(const WiFiCredentials& credentials) {
  preferences_.putString("wifi_ssid", credentials.ssid);
  preferences_.putString("wifi_pass", credentials.password);
}

void ConfigStore::clearWiFiCredentials() {
  preferences_.remove("wifi_ssid");
  preferences_.remove("wifi_pass");
}

size_t ConfigStore::loadWateringHistory(WateringEvent* events, size_t capacity) {
  if (events == nullptr || capacity == 0) {
    return 0;
  }

  const size_t count = min(static_cast<size_t>(preferences_.getUChar("wh_count", 0)), capacity);
  for (size_t i = 0; i < count; ++i) {
    WateringEvent& event = events[i];
    event.sequence = preferences_.getUInt(wateringHistoryKey(i, "seq").c_str(), 0);
    event.reason = static_cast<WateringReason>(
        preferences_.getUChar(wateringHistoryKey(i, "why").c_str(), 0));
    event.startedAtEpochMs = preferences_.getULong64(wateringHistoryKey(i, "st").c_str(), 0);
    event.endedAtEpochMs = preferences_.getULong64(wateringHistoryKey(i, "et").c_str(), 0);
    event.durationMs = preferences_.getUInt(wateringHistoryKey(i, "dur").c_str(), 0);
    event.startRaw = preferences_.getInt(wateringHistoryKey(i, "sr").c_str(), -1);
    event.startPercent = preferences_.getInt(wateringHistoryKey(i, "sp").c_str(), -1);
    event.endRaw = preferences_.getInt(wateringHistoryKey(i, "er").c_str(), -1);
    event.endPercent = preferences_.getInt(wateringHistoryKey(i, "ep").c_str(), -1);
  }

  return count;
}

void ConfigStore::saveWateringHistory(const WateringEvent* events, size_t count) {
  const size_t boundedCount = min(count, kWateringHistoryLimit);
  preferences_.putUChar("wh_count", static_cast<uint8_t>(boundedCount));

  for (size_t i = 0; i < boundedCount; ++i) {
    const WateringEvent& event = events[i];
    preferences_.putUInt(wateringHistoryKey(i, "seq").c_str(), event.sequence);
    preferences_.putUChar(wateringHistoryKey(i, "why").c_str(),
                          static_cast<uint8_t>(event.reason));
    preferences_.putULong64(wateringHistoryKey(i, "st").c_str(), event.startedAtEpochMs);
    preferences_.putULong64(wateringHistoryKey(i, "et").c_str(), event.endedAtEpochMs);
    preferences_.putUInt(wateringHistoryKey(i, "dur").c_str(), event.durationMs);
    preferences_.putInt(wateringHistoryKey(i, "sr").c_str(), event.startRaw);
    preferences_.putInt(wateringHistoryKey(i, "sp").c_str(), event.startPercent);
    preferences_.putInt(wateringHistoryKey(i, "er").c_str(), event.endRaw);
    preferences_.putInt(wateringHistoryKey(i, "ep").c_str(), event.endPercent);
  }

  for (size_t i = boundedCount; i < kWateringHistoryLimit; ++i) {
    removeWateringEvent(preferences_, i);
  }
}
