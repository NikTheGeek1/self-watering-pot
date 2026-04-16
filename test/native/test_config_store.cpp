#include <gtest/gtest.h>

#include "Preferences.h"
#include "app_constants.h"
#include "config_store.h"
#include "native_test_support.h"

namespace {

WateringEvent makeEvent(uint32_t sequence) {
  WateringEvent event;
  event.sequence = sequence;
  event.reason = sequence % 2 == 0 ? WateringReason::Auto : WateringReason::Manual;
  event.startedAtEpochMs = 1700000000000ULL + sequence;
  event.durationMs = 1000 + sequence;
  event.startRaw = 3000 - static_cast<int>(sequence);
  event.startPercent = 40 - static_cast<int>(sequence);
  event.endRaw = 2900 - static_cast<int>(sequence);
  event.endPercent = 50 - static_cast<int>(sequence);
  return event;
}

class ConfigStoreTest : public ::testing::Test {
 protected:
  void SetUp() override { native_test::resetAll(); }
};

}  // namespace

TEST_F(ConfigStoreTest, LoadsDefaultPlantSettingsWhenPreferencesAreEmpty) {
  ConfigStore store;
  ASSERT_TRUE(store.begin());

  const PlantSettings settings = store.loadPlantSettings();
  EXPECT_EQ(settings.dryRaw, -1);
  EXPECT_EQ(settings.wetRaw, -1);
  EXPECT_EQ(settings.dryThresholdPercent, kDefaultDryThresholdPercent);
  EXPECT_EQ(settings.pumpPulseMs, kDefaultPumpPulseMs);
  EXPECT_EQ(settings.cooldownMs, kDefaultCooldownMs);
  EXPECT_EQ(settings.sampleIntervalMs, kDefaultSampleIntervalMs);
  EXPECT_FALSE(settings.autoEnabled);
}

TEST_F(ConfigStoreTest, LoadsClampedPlantSettingsFromPreferences) {
  Preferences prefs;
  ASSERT_TRUE(prefs.begin(kPreferencesNamespace, false));
  prefs.putInt("dry_raw", 3200);
  prefs.putInt("wet_raw", 1800);
  prefs.putUChar("threshold", 250);
  prefs.putUInt("pulse_ms", 1);
  prefs.putUInt("cool_ms", kMaxCooldownMs + 1000);
  prefs.putUInt("sample_ms", 2);

  ConfigStore store;
  ASSERT_TRUE(store.begin());

  const PlantSettings settings = store.loadPlantSettings();
  EXPECT_EQ(settings.dryRaw, 3200);
  EXPECT_EQ(settings.wetRaw, 1800);
  EXPECT_EQ(settings.dryThresholdPercent, kMaxThresholdPercent);
  EXPECT_EQ(settings.pumpPulseMs, kMinPumpPulseMs);
  EXPECT_EQ(settings.cooldownMs, kMaxCooldownMs);
  EXPECT_EQ(settings.sampleIntervalMs, kMinSampleIntervalMs);
  EXPECT_FALSE(settings.autoEnabled);
}

TEST_F(ConfigStoreTest, SavesLoadsAndClearsWiFiCredentialsWithoutTouchingPlantSettings) {
  ConfigStore store;
  ASSERT_TRUE(store.begin());

  PlantSettings settings = store.loadPlantSettings();
  settings.dryRaw = 3000;
  settings.wetRaw = 1600;
  store.savePlantSettings(settings);

  store.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  WiFiCredentials credentials = store.loadWiFiCredentials();
  EXPECT_EQ(credentials.ssid, "GardenNet");
  EXPECT_EQ(credentials.password, "secret");

  store.clearWiFiCredentials();
  credentials = store.loadWiFiCredentials();
  EXPECT_TRUE(credentials.ssid.isEmpty());
  EXPECT_TRUE(credentials.password.isEmpty());

  const PlantSettings reloaded = store.loadPlantSettings();
  EXPECT_EQ(reloaded.dryRaw, 3000);
  EXPECT_EQ(reloaded.wetRaw, 1600);
}

TEST_F(ConfigStoreTest, SavesWateringHistoryWithRetentionAndRemovesLegacyEndTimeKeys) {
  Preferences prefs;
  ASSERT_TRUE(prefs.begin(kPreferencesNamespace, false));
  prefs.putULong64("wh0_et", 12345);

  ConfigStore store;
  ASSERT_TRUE(store.begin());

  WateringEvent events[6];
  for (size_t i = 0; i < 6; ++i) {
    events[i] = makeEvent(static_cast<uint32_t>(i + 1));
  }

  store.saveWateringHistory(events, 6);

  WateringEvent loaded[kWateringHistoryLimit];
  const size_t count = store.loadWateringHistory(loaded, kWateringHistoryLimit);
  ASSERT_EQ(count, kWateringHistoryLimit);
  EXPECT_EQ(loaded[0].sequence, 1u);
  EXPECT_EQ(loaded[4].sequence, 5u);
  EXPECT_FALSE(prefs.isKey("wh0_et"));
  EXPECT_FALSE(prefs.isKey("wh5_seq"));
}

TEST_F(ConfigStoreTest, SavePlantSettingsRoundTripsExpectedValues) {
  ConfigStore store;
  ASSERT_TRUE(store.begin());

  PlantSettings settings;
  settings.dryRaw = 3150;
  settings.wetRaw = 1520;
  settings.dryThresholdPercent = 47;
  settings.pumpPulseMs = 1750;
  settings.cooldownMs = 88000;
  settings.sampleIntervalMs = 9000;
  store.savePlantSettings(settings);

  const PlantSettings loaded = store.loadPlantSettings();
  EXPECT_EQ(loaded.dryRaw, 3150);
  EXPECT_EQ(loaded.wetRaw, 1520);
  EXPECT_EQ(loaded.dryThresholdPercent, 47);
  EXPECT_EQ(loaded.pumpPulseMs, 1750u);
  EXPECT_EQ(loaded.cooldownMs, 88000u);
  EXPECT_EQ(loaded.sampleIntervalMs, 9000u);
}

TEST_F(ConfigStoreTest, LoadWateringHistoryReturnsZeroWhenCountIsMissingOrCapacityIsZero) {
  ConfigStore store;
  ASSERT_TRUE(store.begin());

  WateringEvent events[kWateringHistoryLimit];
  EXPECT_EQ(store.loadWateringHistory(events, kWateringHistoryLimit), 0u);
  EXPECT_EQ(store.loadWateringHistory(events, 0), 0u);
  EXPECT_EQ(store.loadWateringHistory(nullptr, kWateringHistoryLimit), 0u);
}
