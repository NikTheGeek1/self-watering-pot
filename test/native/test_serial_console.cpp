#include <gtest/gtest.h>

#include <memory>

#include "config_store.h"
#include "native_test_support.h"
#include "network_manager.h"
#include "plant_controller.h"
#include "serial_console.h"

namespace {

time_t fakeSeconds() { return 1705000000; }
bool fakeTimespec(struct timespec* out) {
  if (out == nullptr) {
    return false;
  }
  out->tv_sec = 1705000000;
  out->tv_nsec = 0;
  return true;
}
void fakeConfigure(const char*, const char*, const char*) {}

const TimeServiceOps kSerialTimeOps = {
    fakeConfigure,
    fakeSeconds,
    fakeTimespec,
};

class SerialConsoleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    native_test::resetAll();
    ASSERT_TRUE(store_.begin());
    native_test::setAnalogValue(kMoisturePin, 2800);
    plant_ = std::make_unique<PlantController>(store_, time_);
    network_ = std::make_unique<NetworkManager>(store_, *plant_, time_);
    console_ = std::make_unique<SerialConsole>(*plant_, *network_);
    plant_->begin();
    network_->begin();
    console_->begin();
    native_test::clearSerial();
  }

  ConfigStore store_;
  TimeService time_{&kSerialTimeOps};
  std::unique_ptr<PlantController> plant_;
  std::unique_ptr<NetworkManager> network_;
  std::unique_ptr<SerialConsole> console_;
};

}  // namespace

TEST_F(SerialConsoleTest, PumpCommandStartsOneManualPulse) {
  native_test::queueSerialInput("pump\n");
  console_->tick();

  const PlantStatusSnapshot status = plant_->snapshot(native_test::currentMillis());
  EXPECT_TRUE(status.pumpRunning);
  EXPECT_NE(native_test::serialOutput().find("Pump start (manual)"), std::string::npos);
}

TEST_F(SerialConsoleTest, WifiClearKeepsDeviceInSetupMode) {
  store_.saveWiFiCredentials(WiFiCredentials{"Garden", "secret"});
  network_->begin();
  native_test::clearSerial();

  native_test::queueSerialInput("wifi clear\n");
  console_->tick();

  const NetworkStatusSnapshot status = network_->snapshot();
  EXPECT_EQ(status.state, WiFiState::SetupAp);
  EXPECT_TRUE(store_.loadWiFiCredentials().ssid.isEmpty());
}

TEST_F(SerialConsoleTest, HelpAndUnknownCommandsPrintExpectedOutput) {
  native_test::queueSerialInput("help\nnope\n");
  console_->tick();

  const std::string output = native_test::serialOutput();
  EXPECT_NE(output.find("Commands:"), std::string::npos);
  EXPECT_NE(output.find("Unknown command: nope"), std::string::npos);
}

TEST_F(SerialConsoleTest, ReadAndStatusCommandsPrintCurrentState) {
  native_test::queueSerialInput("read\nstatus\n");
  console_->tick();

  const std::string output = native_test::serialOutput();
  EXPECT_NE(output.find("Manual reading"), std::string::npos);
  EXPECT_NE(output.find("Plant Status"), std::string::npos);
  EXPECT_NE(output.find("Network Status"), std::string::npos);
}

TEST_F(SerialConsoleTest, SetCommandsClampAndPersistSettings) {
  native_test::queueSerialInput("set threshold 1\nset pulse 99999\nset cooldown 1\nset sample 999999\n");
  console_->tick();

  const PlantStatusSnapshot status = plant_->snapshot(native_test::currentMillis());
  EXPECT_EQ(status.settings.dryThresholdPercent, kMinThresholdPercent);
  EXPECT_EQ(status.settings.pumpPulseMs, kMaxPumpPulseMs);
  EXPECT_EQ(status.settings.cooldownMs, kMinCooldownMs);
  EXPECT_EQ(status.settings.sampleIntervalMs, kMaxSampleIntervalMs);
}

TEST_F(SerialConsoleTest, CalibrationAndAutoCommandsUpdateControllerState) {
  native_test::setAnalogValue(kMoisturePin, 3200);
  native_test::queueSerialInput("cal dry\n");
  console_->tick();

  native_test::setAnalogValue(kMoisturePin, 1600);
  native_test::queueSerialInput("cal wet\nauto on\nauto off\n");
  console_->tick();

  const PlantStatusSnapshot status = plant_->snapshot(native_test::currentMillis());
  EXPECT_EQ(status.settings.dryRaw, 3200);
  EXPECT_EQ(status.settings.wetRaw, 1600);
  EXPECT_FALSE(status.settings.autoEnabled);
  EXPECT_NE(native_test::serialOutput().find("Auto mode enabled"), std::string::npos);
  EXPECT_NE(native_test::serialOutput().find("Auto mode disabled"), std::string::npos);
}

TEST_F(SerialConsoleTest, DiagnosticCommandRunsHighThenLowSweep) {
  native_test::queueSerialInput("diag\n");
  console_->tick();

  EXPECT_EQ(native_test::digitalValue(kPumpPin), LOW);
  EXPECT_GE(native_test::currentMillis(), 4000u);
  EXPECT_NE(native_test::serialOutput().find("Diagnostic sweep start"), std::string::npos);
  EXPECT_NE(native_test::serialOutput().find("Diagnostic sweep end"), std::string::npos);
}

TEST_F(SerialConsoleTest, AutoOnPrintsErrorWhenCalibrationIsMissing) {
  native_test::queueSerialInput("auto on\n");
  console_->tick();

  EXPECT_NE(native_test::serialOutput().find("Auto mode requires both dry and wet calibration values."),
            std::string::npos);
}

TEST_F(SerialConsoleTest, AliasesAreAcceptedForHelpReadStatusAndPump) {
  native_test::queueSerialInput("?\nm\ns\np\n");
  console_->tick();

  const std::string output = native_test::serialOutput();
  EXPECT_NE(output.find("Commands:"), std::string::npos);
  EXPECT_NE(output.find("Manual reading"), std::string::npos);
  EXPECT_NE(output.find("Network Status"), std::string::npos);
  EXPECT_TRUE(plant_->snapshot(native_test::currentMillis()).pumpRunning);
}
