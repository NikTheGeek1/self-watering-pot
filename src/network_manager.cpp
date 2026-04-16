#include "network_manager.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "app_constants.h"
#include "web_content.h"

namespace {

String jsonEscape(const String& value) {
  String escaped = value;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "\\r");
  return escaped;
}

bool parseUnsigned(const String& value, unsigned long* out) {
  if (value.isEmpty()) {
    return false;
  }

  char* endPtr = nullptr;
  const unsigned long parsed = strtoul(value.c_str(), &endPtr, 10);
  if (endPtr == value.c_str() || *endPtr != '\0') {
    return false;
  }

  *out = parsed;
  return true;
}

String otaErrorMessage(ota_error_t error) {
  switch (error) {
    case OTA_AUTH_ERROR:
      return F("OTA authentication failed");
    case OTA_BEGIN_ERROR:
      return F("OTA begin failed");
    case OTA_CONNECT_ERROR:
      return F("OTA connection failed");
    case OTA_RECEIVE_ERROR:
      return F("OTA receive failed");
    case OTA_END_ERROR:
      return F("OTA end failed");
    default:
      return F("OTA failed");
  }
}

void appendWateringHistoryJson(String& json, const PlantStatusSnapshot& plant) {
  json += F(",\"wateringHistory\":[");

  for (size_t i = 0; i < plant.wateringHistoryCount; ++i) {
    if (i > 0) {
      json += ',';
    }

    const WateringEvent& event = plant.wateringHistory[i];
    json += F("{\"sequence\":");
    json += event.sequence;
    json += F(",\"reason\":\"");
    json += wateringReasonToText(event.reason);
    json += F("\",\"startedAtEpochMs\":");
    json += event.startedAtEpochMs;
    json += F(",\"endedAtEpochMs\":");
    json += event.endedAtEpochMs;
    json += F(",\"durationMs\":");
    json += event.durationMs;
    json += F(",\"startRaw\":");
    json += event.startRaw;
    json += F(",\"startPercent\":");
    json += event.startPercent;
    json += F(",\"endRaw\":");
    json += event.endRaw;
    json += F(",\"endPercent\":");
    json += event.endPercent;
    json += '}';
  }

  json += ']';
}

}  // namespace

NetworkManager::NetworkManager(ConfigStore& configStore, PlantController& plantController,
                               TimeService& timeService)
    : configStore_(configStore),
      plantController_(plantController),
      timeService_(timeService),
      server_(80) {}

void NetworkManager::begin() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_OFF);

  configureRoutes();
  configureOta();

  credentials_ = configStore_.loadWiFiCredentials();
  if (credentials_.isConfigured()) {
    beginStationAttempt(true);
  } else {
    startSetupAccessPoint(F("No Wi-Fi credentials stored. Enter local Wi-Fi details to continue."));
  }
}

void NetworkManager::tick(unsigned long now) {
  if (serverStarted_) {
    server_.handleClient();
  }

  if (dnsActive_) {
    dnsServer_.processNextRequest();
  }

  if (pendingStationStart_) {
    pendingStationStart_ = false;
    beginStationAttempt(true);
  }

  updateStateMachine(now);
  timeService_.tick(now, WiFi.isConnected());

  if (state_ == WiFiState::StaConnected || state_ == WiFiState::OtaInProgress) {
    ArduinoOTA.handle();
  }
}

void NetworkManager::clearCredentialsAndEnterSetupMode() {
  configStore_.clearWiFiCredentials();
  credentials_ = WiFiCredentials{};
  statusMessage_ = F("Wi-Fi credentials cleared from Preferences.");
  startSetupAccessPoint(F("Wi-Fi credentials cleared. Enter new local Wi-Fi details."));
}

NetworkStatusSnapshot NetworkManager::snapshot() const {
  NetworkStatusSnapshot status;
  status.state = state_;
  status.hasCredentials = credentials_.isConfigured();
  status.otaAvailable = (state_ == WiFiState::StaConnected || state_ == WiFiState::OtaInProgress);
  status.otaInProgress = (state_ == WiFiState::OtaInProgress);
  status.failedAttempts = failedAttempts_;
  status.wifiSsid = WiFi.isConnected() ? WiFi.SSID() : credentials_.ssid;
  status.apSsid = apSsid_;
  status.ipAddress = WiFi.isConnected() ? WiFi.localIP().toString() : String();
  status.mdnsName = (WiFi.isConnected() && mdnsActive_) ? String(kMdnsName) : String();
  status.statusMessage = statusMessage_;
  return status;
}

void NetworkManager::printStatus(Stream& out) const {
  const NetworkStatusSnapshot status = snapshot();

  out.println(F("Network Status"));
  out.print(F("  State: "));
  out.println(wifiStateToText(status.state));
  out.print(F("  Stored Wi-Fi SSID: "));
  out.println(status.hasCredentials ? status.wifiSsid : F("unset"));
  out.print(F("  Setup AP SSID: "));
  out.println(status.apSsid.isEmpty() ? F("inactive") : status.apSsid);
  out.print(F("  IP address: "));
  out.println(status.ipAddress.isEmpty() ? F("disconnected") : status.ipAddress);
  out.print(F("  mDNS name: "));
  out.println(status.mdnsName.isEmpty() ? F("inactive") : status.mdnsName);
  out.print(F("  OTA available: "));
  out.println(status.otaAvailable ? F("YES") : F("NO"));
  out.print(F("  Status note: "));
  out.println(status.statusMessage.isEmpty() ? F("none") : status.statusMessage);
  out.println();
}

bool NetworkManager::shouldShowError() const { return errorIndicator_; }

const __FlashStringHelper* NetworkManager::wifiStateToText(WiFiState state) {
  switch (state) {
    case WiFiState::NoCredentials:
      return F("NoCredentials");
    case WiFiState::SetupAp:
      return F("SetupAp");
    case WiFiState::StaConnecting:
      return F("StaConnecting");
    case WiFiState::StaConnected:
      return F("StaConnected");
    case WiFiState::StaError:
      return F("StaError");
    case WiFiState::OtaInProgress:
      return F("OtaInProgress");
  }

  return F("Unknown");
}

void NetworkManager::configureRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatusApi(); });
  server_.on("/api/settings", HTTP_POST, [this]() { handleSettingsApi(); });
  server_.on("/api/calibration/dry", HTTP_POST, [this]() { handleCalibrationDryApi(); });
  server_.on("/api/calibration/wet", HTTP_POST, [this]() { handleCalibrationWetApi(); });
  server_.on("/api/calibration/clear", HTTP_POST, [this]() { handleCalibrationClearApi(); });
  server_.on("/api/provision", HTTP_POST, [this]() { handleProvisionApi(); });
  server_.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/fwlink", HTTP_GET, [this]() { handleRoot(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

void NetworkManager::configureOta() {
  ArduinoOTA.setHostname(kHostname);
  ArduinoOTA.setPassword(kSharedSecret);
  ArduinoOTA.onStart([this]() {
    plantController_.enterOtaLock(&Serial);
    state_ = WiFiState::OtaInProgress;
    errorIndicator_ = false;
    statusMessage_ = F("OTA update in progress.");
  });
  ArduinoOTA.onEnd([this]() { statusMessage_ = F("OTA update finished. Rebooting."); });
  ArduinoOTA.onError([this](ota_error_t error) {
    state_ = WiFi.isConnected() ? WiFiState::StaConnected : WiFiState::StaError;
    errorIndicator_ = true;
    statusMessage_ = otaErrorMessage(error);
  });
}

void NetworkManager::startWebServer() {
  if (serverStarted_) {
    return;
  }

  server_.begin();
  serverStarted_ = true;
}

void NetworkManager::stopWebServer() {
  if (!serverStarted_) {
    return;
  }

  server_.stop();
  serverStarted_ = false;
}

void NetworkManager::handleRoot() {
  if (state_ == WiFiState::SetupAp) {
    server_.send(200, "text/html", buildProvisioningPage(apSsid_, statusMessage_));
    return;
  }

  if (!authorizeStaRequest()) {
    return;
  }

  server_.send(200, "text/html", buildDashboardPage());
}

void NetworkManager::handleStatusApi() {
  if (state_ != WiFiState::SetupAp && !authorizeStaRequest()) {
    return;
  }

  server_.send(200, "application/json", buildStatusJson());
}

void NetworkManager::handleSettingsApi() {
  if (!authorizeStaRequest()) {
    return;
  }

  if (state_ == WiFiState::OtaInProgress) {
    sendJsonResult(503, false, F("Settings are temporarily locked while OTA is in progress."));
    return;
  }

  unsigned long threshold = 0;
  unsigned long pulseMs = 0;
  unsigned long cooldownMs = 0;
  unsigned long sampleMs = 0;
  if (!parseUnsigned(server_.arg("threshold"), &threshold) ||
      !parseUnsigned(server_.arg("pulseMs"), &pulseMs) ||
      !parseUnsigned(server_.arg("cooldownMs"), &cooldownMs) ||
      !parseUnsigned(server_.arg("sampleMs"), &sampleMs)) {
    sendJsonResult(400, false, F("Invalid settings payload."));
    return;
  }

  plantController_.setDryThresholdPercent(static_cast<uint8_t>(threshold));
  plantController_.setPumpPulseMs(static_cast<uint32_t>(pulseMs));
  plantController_.setCooldownMs(static_cast<uint32_t>(cooldownMs));
  plantController_.setSampleIntervalMs(static_cast<uint32_t>(sampleMs));

  String autoError;
  const bool autoEnabled = server_.arg("autoEnabled") == "1";
  if (!plantController_.setAutoMode(autoEnabled, &autoError)) {
    sendJsonResult(409, false, autoError);
    return;
  }

  sendJsonResult(200, true, F("Settings updated."));
}

void NetworkManager::handleCalibrationDryApi() {
  if (!authorizeStaRequest()) {
    return;
  }

  if (state_ == WiFiState::OtaInProgress) {
    sendJsonResult(503, false, F("Calibration is temporarily locked while OTA is in progress."));
    return;
  }

  plantController_.captureCalibrationPoint(true);
  sendJsonResult(200, true, F("Dry calibration saved from the current reading."));
}

void NetworkManager::handleCalibrationWetApi() {
  if (!authorizeStaRequest()) {
    return;
  }

  if (state_ == WiFiState::OtaInProgress) {
    sendJsonResult(503, false, F("Calibration is temporarily locked while OTA is in progress."));
    return;
  }

  plantController_.captureCalibrationPoint(false);
  sendJsonResult(200, true, F("Wet calibration saved from the current reading."));
}

void NetworkManager::handleCalibrationClearApi() {
  if (!authorizeStaRequest()) {
    return;
  }

  if (state_ == WiFiState::OtaInProgress) {
    sendJsonResult(503, false, F("Calibration is temporarily locked while OTA is in progress."));
    return;
  }

  plantController_.clearCalibration();
  sendJsonResult(200, true, F("Calibration cleared and auto mode disabled."));
}

void NetworkManager::handleProvisionApi() {
  if (state_ != WiFiState::SetupAp) {
    sendJsonResult(409, false, F("Provisioning is only available while setup AP mode is active."));
    return;
  }

  const String ssid = server_.arg("ssid");
  const String password = server_.arg("password");
  if (ssid.isEmpty()) {
    server_.send(400, "text/html", buildProvisioningPage(apSsid_, F("Wi-Fi SSID is required.")));
    return;
  }

  credentials_.ssid = ssid;
  credentials_.password = password;
  configStore_.saveWiFiCredentials(credentials_);
  statusMessage_ = String(F("Saved Wi-Fi credentials for ")) + ssid +
                   F(". The Smart Pot is attempting to join the network now.");

  server_.send(200, "text/html",
               buildProvisioningPage(apSsid_, statusMessage_ +
                                                  F(" If the AP comes back, reconnect and try again.")));
  pendingStationStart_ = true;
}

void NetworkManager::handleNotFound() {
  if (state_ == WiFiState::SetupAp) {
    server_.sendHeader("Location", "/", true);
    server_.send(302, "text/plain", "");
    return;
  }

  server_.send(404, "application/json", "{\"ok\":false,\"message\":\"Not found.\"}");
}

void NetworkManager::sendJsonResult(int statusCode, bool ok, const String& message) {
  String json;
  json.reserve(96 + message.length());
  json += F("{\"ok\":");
  json += ok ? F("true") : F("false");
  json += F(",\"message\":\"");
  json += jsonEscape(message);
  json += F("\"}");
  server_.send(statusCode, "application/json", json);
}

String NetworkManager::buildStatusJson() const {
  const PlantStatusSnapshot plant = plantController_.snapshot(millis());
  const NetworkStatusSnapshot network = snapshot();

  String json;
  json.reserve(2048);
  json += F("{");
  json += F("\"deviceLabel\":\"");
  json += jsonEscape(String(kDeviceLabel));
  json += F("\",\"appVersion\":\"");
  json += jsonEscape(String(kAppVersion));
  json += F("\",\"wifiState\":\"");
  json += jsonEscape(String(wifiStateToText(network.state)));
  json += F("\",\"wifiSsid\":\"");
  json += jsonEscape(network.wifiSsid);
  json += F("\",\"apSsid\":\"");
  json += jsonEscape(network.apSsid);
  json += F("\",\"ipAddress\":\"");
  json += jsonEscape(network.ipAddress);
  json += F("\",\"mdnsName\":\"");
  json += jsonEscape(network.mdnsName);
  json += F("\",\"statusMessage\":\"");
  json += jsonEscape(network.statusMessage);
  json += F("\",\"otaAvailable\":");
  json += network.otaAvailable ? F("true") : F("false");
  json += F(",\"otaInProgress\":");
  json += network.otaInProgress ? F("true") : F("false");
  json += F(",\"lastRawReading\":");
  json += plant.lastRawReading;
  json += F(",\"lastMoisturePercent\":");
  json += plant.lastMoisturePercent;
  json += F(",\"dryRaw\":");
  json += plant.settings.dryRaw;
  json += F(",\"wetRaw\":");
  json += plant.settings.wetRaw;
  json += F(",\"autoEnabled\":");
  json += plant.settings.autoEnabled ? F("true") : F("false");
  json += F(",\"pumpRunning\":");
  json += plant.pumpRunning ? F("true") : F("false");
  json += F(",\"dryThresholdPercent\":");
  json += plant.settings.dryThresholdPercent;
  json += F(",\"pumpPulseMs\":");
  json += plant.settings.pumpPulseMs;
  json += F(",\"cooldownMs\":");
  json += plant.settings.cooldownMs;
  json += F(",\"sampleIntervalMs\":");
  json += plant.settings.sampleIntervalMs;
  appendWateringHistoryJson(json, plant);
  json += F("}");
  return json;
}

bool NetworkManager::authorizeStaRequest() {
  if (state_ == WiFiState::SetupAp) {
    return true;
  }

  if (!server_.authenticate(kWebAuthUser, kSharedSecret)) {
    server_.requestAuthentication(BASIC_AUTH, kDeviceLabel, "Smart Pot credentials required.");
    return false;
  }

  return true;
}

void NetworkManager::startSetupAccessPoint(const String& message) {
  stopWebServer();
  stopMdns();
  WiFi.disconnect(true, false);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_AP);

  apSsid_ = setupAccessPointSsid();
  WiFi.softAP(apSsid_.c_str(), kSharedSecret);
  dnsServer_.start(53, "*", WiFi.softAPIP());
  dnsActive_ = true;
  startWebServer();

  state_ = WiFiState::SetupAp;
  errorIndicator_ = !message.isEmpty() && credentials_.isConfigured();
  failedAttempts_ = 0;
  statusMessage_ = message;

  Serial.println();
  Serial.print(F("Setup AP active: "));
  Serial.println(apSsid_);
  Serial.println(F("Open http://192.168.4.1 to provision Wi-Fi."));
}

void NetworkManager::stopSetupAccessPoint() {
  if (dnsActive_) {
    dnsServer_.stop();
    dnsActive_ = false;
  }

  if (!apSsid_.isEmpty()) {
    WiFi.softAPdisconnect(true);
    apSsid_.remove(0);
  }
}

void NetworkManager::beginStationAttempt(bool resetFailures) {
  if (!credentials_.isConfigured()) {
    state_ = WiFiState::NoCredentials;
    startSetupAccessPoint(F("No Wi-Fi credentials stored. Enter local Wi-Fi details to continue."));
    return;
  }

  stopSetupAccessPoint();
  stopWebServer();
  stopMdns();

  if (resetFailures) {
    failedAttempts_ = 0;
  }

  ++failedAttempts_;
  state_ = WiFiState::StaConnecting;
  errorIndicator_ = false;
  statusMessage_ = String(F("Connecting to ")) + credentials_.ssid + F(" (attempt ") +
                   failedAttempts_ + F(" of 3).");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.begin(credentials_.ssid.c_str(), credentials_.password.c_str());
  stationAttemptStartedAtMs_ = millis();
}

void NetworkManager::onStationConnected() {
  stopSetupAccessPoint();
  startWebServer();

  if (!mdnsActive_ && MDNS.begin(kHostname)) {
    MDNS.addService("http", "tcp", 80);
    mdnsActive_ = true;
  }

  if (!otaStarted_) {
    ArduinoOTA.begin();
    otaStarted_ = true;
  }

  timeService_.startSync();

  state_ = WiFiState::StaConnected;
  failedAttempts_ = 0;
  errorIndicator_ = false;
  statusMessage_ = String(F("Connected to ")) + WiFi.SSID() + F(" at ") + WiFi.localIP().toString();

  Serial.println();
  Serial.print(F("Wi-Fi connected: "));
  Serial.println(statusMessage_);
}

void NetworkManager::onStationDisconnected(const String& message) {
  stopMdns();

  if (!credentials_.isConfigured()) {
    startSetupAccessPoint(F("No Wi-Fi credentials stored. Enter local Wi-Fi details to continue."));
    return;
  }

  if (failedAttempts_ >= kWifiConnectAttempts) {
    statusMessage_ = message + F(" Falling back to setup AP.");
    startSetupAccessPoint(statusMessage_);
    return;
  }

  state_ = WiFiState::StaError;
  errorIndicator_ = true;
  statusMessage_ = message;
  nextStationAttemptAtMs_ = millis() + kWifiRetryGapMs;
  WiFi.disconnect(true, false);
}

void NetworkManager::updateStateMachine(unsigned long now) {
  if (state_ == WiFiState::StaConnecting && WiFi.status() == WL_CONNECTED) {
    onStationConnected();
    return;
  }

  if ((state_ == WiFiState::StaConnected || state_ == WiFiState::OtaInProgress) &&
      WiFi.status() != WL_CONNECTED && state_ != WiFiState::OtaInProgress) {
    failedAttempts_ = 0;
    beginStationAttempt(true);
    return;
  }

  if (state_ == WiFiState::StaConnecting &&
      now - stationAttemptStartedAtMs_ >= kWifiConnectTimeoutMs) {
    onStationDisconnected(String(F("Unable to connect to ")) + credentials_.ssid + F(" within 10 seconds."));
    return;
  }

  if (state_ == WiFiState::StaError && now >= nextStationAttemptAtMs_) {
    beginStationAttempt(false);
  }
}

void NetworkManager::stopMdns() {
  if (!mdnsActive_) {
    return;
  }

  MDNS.end();
  mdnsActive_ = false;
}

String NetworkManager::setupAccessPointSsid() const {
  const uint64_t chipId = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04llX", chipId & 0xFFFFULL);
  return String(kSoftApPrefix) + suffix;
}
