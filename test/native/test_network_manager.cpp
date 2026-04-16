#include <gtest/gtest.h>

#include <memory>

#include "ArduinoOTA.h"
#include "ESPmDNS.h"
#include "WebServer.h"
#include "WiFi.h"
#include "app_constants.h"
#include "config_store.h"
#include "native_test_support.h"
#include "network_manager.h"
#include "plant_controller.h"

namespace {

struct FakeTimeEnv {
  time_t seconds = 1705000000;
  uint64_t epochMs = 1705000000000ULL;
  int configureCalls = 0;
} gNetworkTimeEnv;

void fakeConfigure(const char*, const char*, const char*) { ++gNetworkTimeEnv.configureCalls; }
time_t fakeSeconds() { return gNetworkTimeEnv.seconds; }
bool fakeTimespec(struct timespec* out) {
  if (out == nullptr) {
    return false;
  }
  out->tv_sec = static_cast<time_t>(gNetworkTimeEnv.epochMs / 1000ULL);
  out->tv_nsec = 0;
  return true;
}

const TimeServiceOps kNetworkTimeOps = {
    fakeConfigure,
    fakeSeconds,
    fakeTimespec,
};

class NetworkManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    native_test::resetAll();
    ASSERT_TRUE(store_.begin());
    gNetworkTimeEnv = FakeTimeEnv{};
    native_test::setAnalogValue(kMoisturePin, 2800);
    plant_ = std::make_unique<PlantController>(store_, time_);
    plant_->begin();
  }

  ConfigStore store_;
  TimeService time_{&kNetworkTimeOps};
  std::unique_ptr<PlantController> plant_;

  void connect(NetworkManager& manager) {
    WiFi.setStatusForTest(WL_CONNECTED);
    WiFi.setLocalIpForTest(IPAddress(192, 168, 1, 10));
    manager.tick(native_test::currentMillis());
  }
};

}  // namespace

TEST_F(NetworkManagerTest, BeginWithoutCredentialsStartsSetupAp) {
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();

  const NetworkStatusSnapshot status = manager.snapshot();
  EXPECT_EQ(status.state, WiFiState::SetupAp);
  EXPECT_EQ(WiFi.softApPasswordForTest(), kSharedSecret);
  EXPECT_NE(status.apSsid.std().find(kSoftApPrefix), std::string::npos);
}

TEST_F(NetworkManagerTest, SetupApRoutesServeProvisioningPageAndRedirectUnknownPaths) {
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);

  FakeHttpResponse root = server->simulateRequest(HTTP_GET, "/");
  EXPECT_EQ(root.statusCode, 200);
  EXPECT_NE(root.body.std().find("Smart Pot Setup"), std::string::npos);

  FakeHttpResponse captive = server->simulateRequest(HTTP_GET, "/generate_204");
  EXPECT_EQ(captive.statusCode, 200);
  EXPECT_NE(captive.body.std().find("Smart Pot Setup"), std::string::npos);

  FakeHttpResponse notFound = server->simulateRequest(HTTP_GET, "/missing");
  EXPECT_EQ(notFound.statusCode, 302);
  EXPECT_EQ(notFound.headers["Location"], "/");
}

TEST_F(NetworkManagerTest, BeginWithCredentialsStartsStationAttempt) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();

  const NetworkStatusSnapshot status = manager.snapshot();
  EXPECT_EQ(status.state, WiFiState::StaConnecting);
  EXPECT_EQ(WiFi.stationSsidForTest(), "GardenNet");
  EXPECT_EQ(WiFi.hostnameForTest(), kHostname);
}

TEST_F(NetworkManagerTest, SuccessfulStationConnectionEnablesStaServices) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();

  EXPECT_EQ(manager.snapshot().state, WiFiState::StaConnecting);

  connect(manager);

  const NetworkStatusSnapshot status = manager.snapshot();
  EXPECT_EQ(status.state, WiFiState::StaConnected);
  EXPECT_TRUE(ArduinoOTA.startedForTest());
  EXPECT_EQ(ArduinoOTA.hostnameForTest(), kHostname);
  EXPECT_EQ(status.mdnsName, kMdnsName);
  EXPECT_EQ(gNetworkTimeEnv.configureCalls, 1);
}

TEST_F(NetworkManagerTest, FallsBackToSetupApAfterThreeFailedAttempts) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();

  for (int attempt = 0; attempt < 3; ++attempt) {
    native_test::advanceMillis(kWifiConnectTimeoutMs + 1);
    manager.tick(native_test::currentMillis());
    if (attempt < 2) {
      native_test::advanceMillis(kWifiRetryGapMs + 1);
      manager.tick(native_test::currentMillis());
    }
  }

  EXPECT_EQ(manager.snapshot().state, WiFiState::SetupAp);
}

TEST_F(NetworkManagerTest, DisconnectAfterConnectRestartsStationAttemptAndStopsMdns) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  ASSERT_TRUE(MDNS.activeForTest());
  WiFi.setStatusForTest(WL_DISCONNECTED);
  manager.tick(native_test::currentMillis());

  const NetworkStatusSnapshot status = manager.snapshot();
  EXPECT_EQ(status.state, WiFiState::StaConnecting);
  EXPECT_FALSE(MDNS.activeForTest());
}

TEST_F(NetworkManagerTest, ReconnectFailuresAfterDisconnectFallBackToSetupAp) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WiFi.setStatusForTest(WL_DISCONNECTED);
  manager.tick(native_test::currentMillis());
  EXPECT_EQ(manager.snapshot().state, WiFiState::StaConnecting);

  for (int attempt = 0; attempt < 3; ++attempt) {
    native_test::advanceMillis(kWifiConnectTimeoutMs + 1);
    manager.tick(native_test::currentMillis());
    if (attempt < 2) {
      native_test::advanceMillis(kWifiRetryGapMs + 1);
      manager.tick(native_test::currentMillis());
    }
  }

  EXPECT_EQ(manager.snapshot().state, WiFiState::SetupAp);
}

TEST_F(NetworkManagerTest, StaApiRequiresAuthAndManualWaterEndpointWorksWhenAuthorized) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);

  FakeHttpResponse unauthorized =
      server->simulateRequest(HTTP_GET, "/api/status");
  EXPECT_EQ(unauthorized.statusCode, 401);

  FakeHttpResponse manualWater =
      server->simulateRequest(HTTP_POST, "/api/manual-water", {}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(manualWater.statusCode, 200);
  EXPECT_NE(manualWater.body.std().find("Manual watering started"), std::string::npos);
}

TEST_F(NetworkManagerTest, StaRootServesDashboardAfterAuthAndNotFoundReturnsJson404) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);

  FakeHttpResponse root = server->simulateRequest(HTTP_GET, "/", {}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(root.statusCode, 200);
  EXPECT_NE(root.body.std().find("Smart Pot Dashboard"), std::string::npos);

  FakeHttpResponse notFound = server->simulateRequest(HTTP_GET, "/missing");
  EXPECT_EQ(notFound.statusCode, 404);
  EXPECT_NE(notFound.body.std().find("\"message\":\"Not found.\""), std::string::npos);
}

TEST_F(NetworkManagerTest, StatusApiIncludesWateringHistoryAndOmitsEndedAtEpochMs) {
  native_test::setAnalogSequence(kMoisturePin, std::vector<int>(kMoistureSamplesPerRead * 3, 2800));
  ASSERT_TRUE(plant_->runManualPumpPulse());
  native_test::advanceMillis(kDefaultPumpPulseMs + 5);
  plant_->tick(native_test::currentMillis());

  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response =
      server->simulateRequest(HTTP_GET, "/api/status", {}, kWebAuthUser, kSharedSecret);

  EXPECT_EQ(response.statusCode, 200);
  EXPECT_NE(response.body.std().find("\"wateringHistory\""), std::string::npos);
  EXPECT_NE(response.body.std().find("\"startedAtEpochMs\""), std::string::npos);
  EXPECT_EQ(response.body.std().find("endedAtEpochMs"), std::string::npos);
}

TEST_F(NetworkManagerTest, SettingsApiRejectsInvalidPayloadAndInvalidAutoEnable) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);

  FakeHttpResponse invalidPayload = server->simulateRequest(
      HTTP_POST, "/api/settings", {{"threshold", "bad"}}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(invalidPayload.statusCode, 400);

  FakeHttpResponse invalidAuto = server->simulateRequest(
      HTTP_POST, "/api/settings",
      {{"threshold", "35"}, {"pulseMs", "1200"}, {"cooldownMs", "45000"},
       {"sampleMs", "5000"}, {"autoEnabled", "1"}},
      kWebAuthUser, kSharedSecret);
  EXPECT_EQ(invalidAuto.statusCode, 409);
  EXPECT_NE(invalidAuto.body.std().find("Auto mode requires both dry and wet calibration values."),
            std::string::npos);
}

TEST_F(NetworkManagerTest, SettingsApiSuccessPersistsUpdatedValues) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response = server->simulateRequest(
      HTTP_POST, "/api/settings",
      {{"threshold", "44"}, {"pulseMs", "1500"}, {"cooldownMs", "60000"},
       {"sampleMs", "9000"}, {"autoEnabled", "0"}},
      kWebAuthUser, kSharedSecret);

  EXPECT_EQ(response.statusCode, 200);
  const PlantSettings saved = store_.loadPlantSettings();
  EXPECT_EQ(saved.dryThresholdPercent, 44);
  EXPECT_EQ(saved.pumpPulseMs, 1500u);
  EXPECT_EQ(saved.cooldownMs, 60000u);
  EXPECT_EQ(saved.sampleIntervalMs, 9000u);
}

TEST_F(NetworkManagerTest, CalibrationRoutesPersistValuesAndClearDisablesAuto) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);

  native_test::setAnalogValue(kMoisturePin, 3200);
  FakeHttpResponse dry = server->simulateRequest(
      HTTP_POST, "/api/calibration/dry", {}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(dry.statusCode, 200);
  EXPECT_EQ(store_.loadPlantSettings().dryRaw, 3200);

  native_test::setAnalogValue(kMoisturePin, 1600);
  FakeHttpResponse wet = server->simulateRequest(
      HTTP_POST, "/api/calibration/wet", {}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(wet.statusCode, 200);

  String error;
  ASSERT_TRUE(plant_->setAutoMode(true, &error));
  FakeHttpResponse clear = server->simulateRequest(
      HTTP_POST, "/api/calibration/clear", {}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(clear.statusCode, 200);
  const PlantSettings cleared = store_.loadPlantSettings();
  EXPECT_EQ(cleared.dryRaw, -1);
  EXPECT_EQ(cleared.wetRaw, -1);
  EXPECT_FALSE(plant_->snapshot(native_test::currentMillis()).settings.autoEnabled);
}

TEST_F(NetworkManagerTest, CalibrationRoutesReturn503DuringOta) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  ArduinoOTA.triggerStartForTest();

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response = server->simulateRequest(
      HTTP_POST, "/api/calibration/dry", {}, kWebAuthUser, kSharedSecret);

  EXPECT_EQ(manager.snapshot().state, WiFiState::OtaInProgress);
  EXPECT_EQ(response.statusCode, 503);
  EXPECT_TRUE(plant_->snapshot(native_test::currentMillis()).otaLockActive);
}

TEST_F(NetworkManagerTest, ManualWaterRouteRejectsWhenPumpRunningOrOtaIsActive) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  native_test::setAnalogSequence(kMoisturePin, std::vector<int>(kMoistureSamplesPerRead * 6, 2800));
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  ASSERT_TRUE(plant_->runManualPumpPulse());

  FakeHttpResponse running = server->simulateRequest(
      HTTP_POST, "/api/manual-water", {}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(running.statusCode, 409);

  native_test::advanceMillis(kDefaultPumpPulseMs + 5);
  plant_->tick(native_test::currentMillis());
  ArduinoOTA.triggerStartForTest();

  FakeHttpResponse ota = server->simulateRequest(
      HTTP_POST, "/api/manual-water", {}, kWebAuthUser, kSharedSecret);
  EXPECT_EQ(ota.statusCode, 503);
}

TEST_F(NetworkManagerTest, ProvisionApiAcceptsOpenNetworkAndStartsConnecting) {
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response = server->simulateRequest(
      HTTP_POST, "/api/provision", {{"ssid", "OpenGarden"}, {"password", ""}});

  EXPECT_EQ(response.statusCode, 200);
  EXPECT_EQ(store_.loadWiFiCredentials().ssid, "OpenGarden");
  EXPECT_EQ(store_.loadWiFiCredentials().password, "");

  manager.tick(native_test::currentMillis());
  EXPECT_EQ(manager.snapshot().state, WiFiState::StaConnecting);
  EXPECT_EQ(WiFi.stationSsidForTest(), "OpenGarden");
}

TEST_F(NetworkManagerTest, ProvisionApiRejectsOutsideSetupMode) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response = server->simulateRequest(
      HTTP_POST, "/api/provision", {{"ssid", "OtherNet"}}, kWebAuthUser, kSharedSecret);

  EXPECT_EQ(response.statusCode, 409);
}

TEST_F(NetworkManagerTest, OtaErrorReturnsToConnectedStateAndSetsErrorMessage) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  ArduinoOTA.triggerStartForTest();
  EXPECT_EQ(manager.snapshot().state, WiFiState::OtaInProgress);

  ArduinoOTA.triggerErrorForTest(OTA_AUTH_ERROR);
  const NetworkStatusSnapshot status = manager.snapshot();
  EXPECT_EQ(status.state, WiFiState::StaConnected);
  EXPECT_TRUE(manager.shouldShowError());
  EXPECT_EQ(status.statusMessage, "OTA authentication failed");
}

TEST_F(NetworkManagerTest, StatusApiIsAccessibleWithoutAuthInSetupMode) {
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response = server->simulateRequest(HTTP_GET, "/api/status");

  EXPECT_EQ(response.statusCode, 200);
  EXPECT_NE(response.body.std().find("\"wifiState\":\"SetupAp\""), std::string::npos);
}

TEST_F(NetworkManagerTest, UnauthorizedStaRequestsReturnAuthChallengeHeader) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response = server->simulateRequest(HTTP_POST, "/api/settings");

  EXPECT_EQ(response.statusCode, 401);
  EXPECT_NE(response.headers["WWW-Authenticate"].find("Basic realm=\"Smart Pot\""),
            std::string::npos);
}

TEST_F(NetworkManagerTest, SettingsApiReturns503WhileOtaIsInProgress) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);
  ArduinoOTA.triggerStartForTest();

  WebServer* server = WebServer::lastInstance();
  ASSERT_NE(server, nullptr);
  FakeHttpResponse response = server->simulateRequest(
      HTTP_POST, "/api/settings",
      {{"threshold", "44"}, {"pulseMs", "1500"}, {"cooldownMs", "60000"},
       {"sampleMs", "9000"}, {"autoEnabled", "0"}},
      kWebAuthUser, kSharedSecret);

  EXPECT_EQ(response.statusCode, 503);
}

TEST_F(NetworkManagerTest, OtaStartMarksInProgressAndEndUpdatesStatusMessage) {
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});
  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  connect(manager);

  ArduinoOTA.triggerStartForTest();
  NetworkStatusSnapshot status = manager.snapshot();
  EXPECT_EQ(status.state, WiFiState::OtaInProgress);
  EXPECT_TRUE(status.otaInProgress);

  ArduinoOTA.triggerEndForTest();
  status = manager.snapshot();
  EXPECT_EQ(status.statusMessage, "OTA update finished. Rebooting.");
}

TEST_F(NetworkManagerTest, ClearCredentialsAndEnterSetupModeClearsOnlyWifiState) {
  PlantSettings persisted;
  persisted.dryRaw = 3200;
  persisted.wetRaw = 1600;
  persisted.dryThresholdPercent = 48;
  store_.savePlantSettings(persisted);
  store_.saveWiFiCredentials(WiFiCredentials{"GardenNet", "secret"});

  NetworkManager manager(store_, *plant_, time_);
  manager.begin();
  manager.clearCredentialsAndEnterSetupMode();

  EXPECT_EQ(manager.snapshot().state, WiFiState::SetupAp);
  EXPECT_TRUE(store_.loadWiFiCredentials().ssid.isEmpty());
  const PlantSettings loaded = store_.loadPlantSettings();
  EXPECT_EQ(loaded.dryRaw, 3200);
  EXPECT_EQ(loaded.wetRaw, 1600);
  EXPECT_EQ(loaded.dryThresholdPercent, 48);
}
