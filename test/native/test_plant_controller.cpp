#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "app_constants.h"
#include "config_store.h"
#include "native_test_support.h"
#include "plant_controller.h"

namespace {

struct FakeTimeEnv {
  time_t seconds = 1705000000;
  uint64_t epochMs = 1705000000123ULL;
} gPlantTimeEnv;

void fakeConfigure(const char*, const char*, const char*) {}
time_t fakeSeconds() { return gPlantTimeEnv.seconds; }
bool fakeTimespec(struct timespec* out) {
  if (out == nullptr) {
    return false;
  }
  out->tv_sec = static_cast<time_t>(gPlantTimeEnv.epochMs / 1000ULL);
  out->tv_nsec = static_cast<long>((gPlantTimeEnv.epochMs % 1000ULL) * 1000000ULL);
  return true;
}

const TimeServiceOps kPlantTimeOps = {
    fakeConfigure,
    fakeSeconds,
    fakeTimespec,
};

std::vector<int> repeated(int value, size_t count) { return std::vector<int>(count, value); }

class PlantControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    native_test::resetAll();
    ASSERT_TRUE(store_.begin());
    timeService_ = std::make_unique<TimeService>(&kPlantTimeOps);
  }

  PlantSettings calibratedSettings() const {
    PlantSettings settings;
    settings.dryRaw = 3200;
    settings.wetRaw = 1600;
    settings.sampleIntervalMs = 1000;
    settings.cooldownMs = 5000;
    settings.pumpPulseMs = 1200;
    settings.dryThresholdPercent = 40;
    return settings;
  }

  ConfigStore store_;
  std::unique_ptr<TimeService> timeService_;
};

}  // namespace

TEST_F(PlantControllerTest, ManualPumpPulseRecordsWateringHistoryAndCooldown) {
  native_test::setAnalogSequence(kMoisturePin,
                                 repeated(3000, kMoistureSamplesPerRead * 3));
  PlantController controller(store_, *timeService_);
  controller.begin();

  ASSERT_TRUE(controller.runManualPumpPulse());
  EXPECT_EQ(native_test::digitalValue(kPumpPin), HIGH);

  native_test::advanceMillis(kDefaultPumpPulseMs + 10);
  controller.tick(native_test::currentMillis());

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_FALSE(status.pumpRunning);
  EXPECT_EQ(native_test::digitalValue(kPumpPin), LOW);
  ASSERT_EQ(status.wateringHistoryCount, 1u);
  EXPECT_EQ(status.wateringHistory[0].reason, WateringReason::Manual);
  EXPECT_EQ(status.wateringHistory[0].startedAtEpochMs, gPlantTimeEnv.epochMs);
  EXPECT_GE(status.wateringHistory[0].durationMs, kDefaultPumpPulseMs);
  EXPECT_GT(status.cooldownRemainingMs, 0u);
}

TEST_F(PlantControllerTest, AutoModeRequiresValidCalibration) {
  native_test::setAnalogValue(kMoisturePin, 2500);
  PlantController controller(store_, *timeService_);
  controller.begin();

  String error;
  EXPECT_FALSE(controller.setAutoMode(true, &error));
  EXPECT_EQ(error, "Auto mode requires both dry and wet calibration values.");
}

TEST_F(PlantControllerTest, OtaLockRejectsNewManualWatering) {
  native_test::setAnalogValue(kMoisturePin, 2500);
  PlantController controller(store_, *timeService_);
  controller.begin();
  controller.enterOtaLock();

  EXPECT_FALSE(controller.runManualPumpPulse());
  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_TRUE(status.otaLockActive);
  EXPECT_FALSE(status.pumpRunning);
}

TEST_F(PlantControllerTest, BeginLoadsPersistedSettingsAndWateringHistory) {
  PlantSettings settings = calibratedSettings();
  settings.dryThresholdPercent = 55;
  settings.pumpPulseMs = 1800;
  store_.savePlantSettings(settings);

  WateringEvent event;
  event.sequence = 12;
  event.reason = WateringReason::Auto;
  event.startedAtEpochMs = 1705000000123ULL;
  event.durationMs = 900;
  event.startRaw = 3000;
  event.startPercent = 10;
  event.endRaw = 2900;
  event.endPercent = 20;
  store_.saveWateringHistory(&event, 1);

  native_test::setAnalogValue(kMoisturePin, 2500);
  PlantController controller(store_, *timeService_);
  controller.begin();

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_EQ(status.settings.dryThresholdPercent, 55);
  EXPECT_EQ(status.settings.pumpPulseMs, 1800u);
  EXPECT_EQ(status.lastRawReading, 2500);
  EXPECT_EQ(status.wateringHistoryCount, 1u);
  EXPECT_EQ(status.wateringHistory[0].sequence, 12u);
  EXPECT_FALSE(status.pumpRunning);
}

TEST_F(PlantControllerTest, CaptureCalibrationPointsAndClearCalibrationUpdateStatus) {
  native_test::setAnalogValue(kMoisturePin, 3200);
  PlantController controller(store_, *timeService_);
  controller.begin();

  controller.captureCalibrationPoint(true);
  EXPECT_EQ(controller.snapshot(native_test::currentMillis()).settings.dryRaw, 3200);

  native_test::setAnalogValue(kMoisturePin, 1600);
  controller.captureCalibrationPoint(false);
  PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_EQ(status.settings.wetRaw, 1600);
  EXPECT_TRUE(status.calibrationValid);

  controller.clearCalibration();
  status = controller.snapshot(native_test::currentMillis());
  EXPECT_EQ(status.settings.dryRaw, -1);
  EXPECT_EQ(status.settings.wetRaw, -1);
  EXPECT_FALSE(status.settings.autoEnabled);
  EXPECT_FALSE(status.calibrationValid);
}

TEST_F(PlantControllerTest, SetterMethodsClampToConfiguredRanges) {
  native_test::setAnalogValue(kMoisturePin, 2800);
  PlantController controller(store_, *timeService_);
  controller.begin();

  controller.setDryThresholdPercent(1);
  controller.setPumpPulseMs(99999);
  controller.setCooldownMs(1);
  controller.setSampleIntervalMs(999999);

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_EQ(status.settings.dryThresholdPercent, kMinThresholdPercent);
  EXPECT_EQ(status.settings.pumpPulseMs, kMaxPumpPulseMs);
  EXPECT_EQ(status.settings.cooldownMs, kMinCooldownMs);
  EXPECT_EQ(status.settings.sampleIntervalMs, kMaxSampleIntervalMs);
}

TEST_F(PlantControllerTest, AutoWateringStartsAfterSampleIntervalWhenBelowThreshold) {
  store_.savePlantSettings(calibratedSettings());
  native_test::setAnalogSequence(kMoisturePin, repeated(2800, kMoistureSamplesPerRead * 4));
  PlantController controller(store_, *timeService_);
  controller.begin();

  String error;
  ASSERT_TRUE(controller.setAutoMode(true, &error)) << error.std();

  native_test::advanceMillis(1001);
  controller.tick(native_test::currentMillis());

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_TRUE(status.pumpRunning);
  EXPECT_EQ(native_test::digitalValue(kPumpPin), HIGH);
}

TEST_F(PlantControllerTest, AutoWateringSuspendsWhenCalibrationBecomesInvalidAtRuntime) {
  store_.savePlantSettings(calibratedSettings());
  native_test::setAnalogValue(kMoisturePin, 2800);
  PlantController controller(store_, *timeService_);
  controller.begin();

  String error;
  ASSERT_TRUE(controller.setAutoMode(true, &error));

  native_test::setAnalogValue(kMoisturePin, 1600);
  controller.captureCalibrationPoint(true);

  native_test::advanceMillis(1001);
  controller.tick(native_test::currentMillis());

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_FALSE(status.settings.autoEnabled);
  EXPECT_FALSE(status.pumpRunning);
}

TEST_F(PlantControllerTest, AutoWateringDoesNotRunDuringCooldown) {
  store_.savePlantSettings(calibratedSettings());
  native_test::setAnalogSequence(kMoisturePin, repeated(2800, kMoistureSamplesPerRead * 8));
  PlantController controller(store_, *timeService_);
  controller.begin();

  String error;
  ASSERT_TRUE(controller.setAutoMode(true, &error));
  ASSERT_TRUE(controller.runManualPumpPulse());
  native_test::advanceMillis(kDefaultPumpPulseMs + 10);
  controller.tick(native_test::currentMillis());

  native_test::advanceMillis(1001);
  controller.tick(native_test::currentMillis());

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_FALSE(status.pumpRunning);
  EXPECT_GT(status.cooldownRemainingMs, 0u);
  EXPECT_EQ(status.wateringHistoryCount, 1u);
}

TEST_F(PlantControllerTest, EnterOtaLockStopsActivePumpAndDisablesAutoMode) {
  store_.savePlantSettings(calibratedSettings());
  native_test::setAnalogSequence(kMoisturePin, repeated(2800, kMoistureSamplesPerRead * 6));
  PlantController controller(store_, *timeService_);
  controller.begin();

  String error;
  ASSERT_TRUE(controller.setAutoMode(true, &error));
  ASSERT_TRUE(controller.runManualPumpPulse());

  controller.enterOtaLock();

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  EXPECT_TRUE(status.otaLockActive);
  EXPECT_FALSE(status.pumpRunning);
  EXPECT_FALSE(status.settings.autoEnabled);
  EXPECT_EQ(native_test::digitalValue(kPumpPin), LOW);
  EXPECT_EQ(status.wateringHistoryCount, 1u);
}

TEST_F(PlantControllerTest, ReadNowRefreshesCurrentMoistureSnapshot) {
  native_test::setAnalogValue(kMoisturePin, 3000);
  PlantController controller(store_, *timeService_);
  controller.begin();

  native_test::setAnalogValue(kMoisturePin, 2500);
  controller.readNow();

  EXPECT_EQ(controller.snapshot(native_test::currentMillis()).lastRawReading, 2500);
}

TEST_F(PlantControllerTest, SetAutoModeFalseAlwaysSucceeds) {
  native_test::setAnalogValue(kMoisturePin, 2800);
  PlantController controller(store_, *timeService_);
  controller.begin();

  controller.enterOtaLock();
  String error;
  EXPECT_TRUE(controller.setAutoMode(false, &error));
  EXPECT_TRUE(error.isEmpty());
}

TEST_F(PlantControllerTest, ManualPumpPulseIsRejectedWhileAlreadyRunning) {
  native_test::setAnalogSequence(kMoisturePin, repeated(2800, kMoistureSamplesPerRead * 4));
  PlantController controller(store_, *timeService_);
  controller.begin();

  ASSERT_TRUE(controller.runManualPumpPulse());
  EXPECT_FALSE(controller.runManualPumpPulse());
}

TEST_F(PlantControllerTest, AutoWateringDoesNotTriggerBeforeSampleIntervalOrWhenDisabled) {
  store_.savePlantSettings(calibratedSettings());
  native_test::setAnalogSequence(kMoisturePin, repeated(2800, kMoistureSamplesPerRead * 4));
  PlantController controller(store_, *timeService_);
  controller.begin();

  native_test::advanceMillis(500);
  controller.tick(native_test::currentMillis());
  EXPECT_FALSE(controller.snapshot(native_test::currentMillis()).pumpRunning);

  native_test::advanceMillis(600);
  controller.tick(native_test::currentMillis());
  EXPECT_FALSE(controller.snapshot(native_test::currentMillis()).pumpRunning);
}

TEST_F(PlantControllerTest, AutoWateringDoesNotRunWhenMoistureIsAboveThreshold) {
  PlantSettings settings = calibratedSettings();
  settings.dryThresholdPercent = 35;
  store_.savePlantSettings(settings);
  native_test::setAnalogSequence(kMoisturePin, repeated(2200, kMoistureSamplesPerRead * 4));
  PlantController controller(store_, *timeService_);
  controller.begin();

  String error;
  ASSERT_TRUE(controller.setAutoMode(true, &error));
  native_test::advanceMillis(1001);
  controller.tick(native_test::currentMillis());

  EXPECT_FALSE(controller.snapshot(native_test::currentMillis()).pumpRunning);
}

TEST_F(PlantControllerTest, AutoWateringDoesNotRunWhilePumpIsAlreadyActiveOrWhenOtaLocked) {
  store_.savePlantSettings(calibratedSettings());
  native_test::setAnalogSequence(kMoisturePin, repeated(2800, kMoistureSamplesPerRead * 8));
  PlantController controller(store_, *timeService_);
  controller.begin();

  String error;
  ASSERT_TRUE(controller.setAutoMode(true, &error));
  ASSERT_TRUE(controller.runManualPumpPulse());
  native_test::advanceMillis(1001);
  controller.tick(native_test::currentMillis());
  EXPECT_TRUE(controller.snapshot(native_test::currentMillis()).pumpRunning);

  native_test::advanceMillis(kDefaultPumpPulseMs + 10);
  controller.tick(native_test::currentMillis());
  controller.enterOtaLock();

  native_test::advanceMillis(1001);
  controller.tick(native_test::currentMillis());
  EXPECT_FALSE(controller.snapshot(native_test::currentMillis()).pumpRunning);
}

TEST_F(PlantControllerTest, WateringHistoryKeepsOnlyFiveMostRecentEvents) {
  native_test::setAnalogSequence(kMoisturePin,
                                 repeated(2800, kMoistureSamplesPerRead * 32));
  PlantController controller(store_, *timeService_);
  controller.begin();

  for (size_t i = 0; i < 6; ++i) {
    gPlantTimeEnv.epochMs = 1705000000000ULL + i;
    ASSERT_TRUE(controller.runManualPumpPulse());
    native_test::advanceMillis(kDefaultPumpPulseMs + 5);
    controller.tick(native_test::currentMillis());
  }

  const PlantStatusSnapshot status = controller.snapshot(native_test::currentMillis());
  ASSERT_EQ(status.wateringHistoryCount, 5u);
  EXPECT_EQ(status.wateringHistory[0].sequence, 6u);
  EXPECT_EQ(status.wateringHistory[4].sequence, 2u);
}
