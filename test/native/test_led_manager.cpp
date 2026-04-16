#include <gtest/gtest.h>

#include "app_constants.h"
#include "led_manager.h"
#include "native_test_support.h"

class LedManagerTest : public ::testing::Test {
 protected:
  void SetUp() override { native_test::resetAll(); }
};

TEST_F(LedManagerTest, SetupApShowsGreenDoubleBlink) {
  LedManager manager;
  manager.begin();

  manager.tick(50, WiFiState::SetupAp, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), HIGH);
  manager.tick(180, WiFiState::SetupAp, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), LOW);
}

TEST_F(LedManagerTest, PumpRunningForcesRedLedOn) {
  LedManager manager;
  manager.begin();

  manager.tick(100, WiFiState::StaConnected, true, true);
  EXPECT_EQ(native_test::digitalValue(kRedLedPin), HIGH);
}

TEST_F(LedManagerTest, OtaInProgressUsesFastGreenBlink) {
  LedManager manager;
  manager.begin();

  manager.tick(50, WiFiState::OtaInProgress, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), HIGH);
  manager.tick(150, WiFiState::OtaInProgress, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), LOW);
}

TEST_F(LedManagerTest, StaConnectingUsesTwoHertzBlink) {
  LedManager manager;
  manager.begin();

  manager.tick(100, WiFiState::StaConnecting, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), HIGH);
  manager.tick(300, WiFiState::StaConnecting, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), LOW);
}

TEST_F(LedManagerTest, StaConnectedUsesHeartbeatPattern) {
  LedManager manager;
  manager.begin();

  manager.tick(100, WiFiState::StaConnected, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), HIGH);
  manager.tick(300, WiFiState::StaConnected, false, false);
  EXPECT_EQ(native_test::digitalValue(kGreenLedPin), LOW);
}

TEST_F(LedManagerTest, ErrorStateShowsRedDoubleBlinkWhenPumpIsIdle) {
  LedManager manager;
  manager.begin();

  manager.tick(50, WiFiState::StaError, false, true);
  EXPECT_EQ(native_test::digitalValue(kRedLedPin), HIGH);
  manager.tick(180, WiFiState::StaError, false, true);
  EXPECT_EQ(native_test::digitalValue(kRedLedPin), LOW);
}
