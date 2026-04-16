#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include <Arduino.h>

#include "config_store.h"
#include "plant_controller.h"
#include "time_service.h"

enum class WiFiState {
  NoCredentials,
  SetupAp,
  StaConnecting,
  StaConnected,
  StaError,
  OtaInProgress,
};

struct NetworkStatusSnapshot {
  WiFiState state = WiFiState::NoCredentials;
  bool hasCredentials = false;
  bool otaAvailable = false;
  bool otaInProgress = false;
  uint8_t failedAttempts = 0;
  String wifiSsid;
  String apSsid;
  String ipAddress;
  String mdnsName;
  String statusMessage;
};

class NetworkManager {
 public:
  NetworkManager(ConfigStore& configStore, PlantController& plantController,
                 TimeService& timeService);

  void begin();
  void tick(unsigned long now);

  void clearCredentialsAndEnterSetupMode();

  NetworkStatusSnapshot snapshot() const;
  void printStatus(Stream& out) const;
  bool shouldShowError() const;

  static const __FlashStringHelper* wifiStateToText(WiFiState state);

 private:
  void configureRoutes();
  void configureOta();
  void startWebServer();
  void stopWebServer();
  void handleRoot();
  void handleStatusApi();
  void handleSettingsApi();
  void handleCalibrationDryApi();
  void handleCalibrationWetApi();
  void handleCalibrationClearApi();
  void handleProvisionApi();
  void handleNotFound();

  void sendJsonResult(int statusCode, bool ok, const String& message);
  String buildStatusJson() const;
  bool authorizeStaRequest();

  void startSetupAccessPoint(const String& message);
  void stopSetupAccessPoint();
  void beginStationAttempt(bool resetFailures);
  void onStationConnected();
  void onStationDisconnected(const String& message);
  void updateStateMachine(unsigned long now);
  void stopMdns();
  String setupAccessPointSsid() const;

  ConfigStore& configStore_;
  PlantController& plantController_;
  TimeService& timeService_;
  WebServer server_;
  DNSServer dnsServer_;

  WiFiState state_ = WiFiState::NoCredentials;
  WiFiCredentials credentials_;
  bool serverStarted_ = false;
  bool dnsActive_ = false;
  bool mdnsActive_ = false;
  bool otaStarted_ = false;
  bool pendingStationStart_ = false;
  bool errorIndicator_ = false;
  uint8_t failedAttempts_ = 0;
  unsigned long stationAttemptStartedAtMs_ = 0;
  unsigned long nextStationAttemptAtMs_ = 0;
  String apSsid_;
  String statusMessage_;
};
