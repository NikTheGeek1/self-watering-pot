#include "WiFi.h"

#include "ArduinoOTA.h"
#include "DNSServer.h"
#include "ESPmDNS.h"
#include "WebServer.h"

WiFiClass WiFi;
ArduinoOTAClass ArduinoOTA;
MDNSClass MDNS;
WebServer* WebServer::lastInstance_ = nullptr;

void WiFiClass::persistent(bool enabled) { persistent_ = enabled; }

void WiFiClass::setAutoReconnect(bool enabled) { autoReconnect_ = enabled; }

void WiFiClass::mode(wifi_mode_t modeValue) { mode_ = modeValue; }

wifi_mode_t WiFiClass::mode() const { return mode_; }

void WiFiClass::begin(const char* ssid, const char* password) {
  stationSsid_ = ssid;
  stationPassword_ = password;
  status_ = WL_DISCONNECTED;
}

void WiFiClass::disconnect(bool, bool) {
  status_ = WL_DISCONNECTED;
  localIp_ = IPAddress(0, 0, 0, 0);
}

bool WiFiClass::isConnected() const { return status_ == WL_CONNECTED; }

wl_status_t WiFiClass::status() const { return status_; }

String WiFiClass::SSID() const { return stationSsid_; }

IPAddress WiFiClass::localIP() const { return localIp_; }

void WiFiClass::setHostname(const char* hostname) { hostname_ = hostname; }

bool WiFiClass::softAP(const char* ssid, const char* password) {
  softApSsid_ = ssid;
  softApPassword_ = password;
  return true;
}

void WiFiClass::softAPdisconnect(bool) {
  softApSsid_.remove(0);
  softApPassword_.remove(0);
}

IPAddress WiFiClass::softAPIP() const { return softApIp_; }

void WiFiClass::setStatusForTest(wl_status_t statusValue) { status_ = statusValue; }

void WiFiClass::setLocalIpForTest(const IPAddress& ipAddress) { localIp_ = ipAddress; }

String WiFiClass::stationSsidForTest() const { return stationSsid_; }

String WiFiClass::softApSsidForTest() const { return softApSsid_; }

String WiFiClass::softApPasswordForTest() const { return softApPassword_; }

String WiFiClass::hostnameForTest() const { return hostname_; }

void WiFiClass::resetForTest() {
  persistent_ = false;
  autoReconnect_ = false;
  mode_ = WIFI_OFF;
  status_ = WL_DISCONNECTED;
  stationSsid_.remove(0);
  stationPassword_.remove(0);
  softApSsid_.remove(0);
  softApPassword_.remove(0);
  hostname_.remove(0);
  localIp_ = IPAddress(0, 0, 0, 0);
}

WebServer::WebServer(int port) : port_(port) { lastInstance_ = this; }

void WebServer::on(const String& path, HTTPMethod method, Handler handler) {
  routes_[RouteKey{path.std(), method}] = std::move(handler);
}

void WebServer::onNotFound(Handler handler) { notFoundHandler_ = std::move(handler); }

void WebServer::begin() { started_ = true; }

void WebServer::stop() { started_ = false; }

void WebServer::handleClient() {}

bool WebServer::authenticate(const char* user, const char* password) {
  return currentAuthUser_ == user && currentAuthPassword_ == password;
}

void WebServer::requestAuthentication(HTTPAuthMethod, const char* realm, const char* message) {
  currentResponse_.statusCode = 401;
  currentResponse_.contentType = "text/plain";
  currentResponse_.body = message;
  currentResponse_.headers["WWW-Authenticate"] =
      std::string("Basic realm=\"") + (realm != nullptr ? realm : "") + "\"";
}

void WebServer::send(int code, const char* contentType, const String& body) {
  currentResponse_.statusCode = code;
  currentResponse_.contentType = contentType;
  currentResponse_.body = body;
}

void WebServer::sendHeader(const char* name, const char* value, bool) {
  currentResponse_.headers[name != nullptr ? name : ""] = value != nullptr ? value : "";
}

String WebServer::arg(const char* name) const {
  const auto it = currentArgs_.find(name != nullptr ? name : "");
  return it == currentArgs_.end() ? String() : it->second;
}

FakeHttpResponse WebServer::simulateRequest(HTTPMethod method, const String& path,
                                            const std::map<std::string, String>& args,
                                            const String& authUser, const String& authPassword) {
  currentMethod_ = method;
  currentPath_ = path;
  currentArgs_ = args;
  currentAuthUser_ = authUser;
  currentAuthPassword_ = authPassword;
  currentResponse_ = FakeHttpResponse{};

  const auto route = routes_.find(RouteKey{path.std(), method});
  if (route != routes_.end()) {
    route->second();
  } else if (notFoundHandler_) {
    notFoundHandler_();
  } else {
    currentResponse_.statusCode = 404;
    currentResponse_.contentType = "text/plain";
    currentResponse_.body = "Not found";
  }

  return currentResponse_;
}

WebServer* WebServer::lastInstance() { return lastInstance_; }

void DNSServer::start(uint16_t, const char*, const IPAddress&) { active_ = true; }

void DNSServer::stop() { active_ = false; }

void DNSServer::processNextRequest() {}

bool DNSServer::activeForTest() const { return active_; }

void ArduinoOTAClass::setHostname(const char* hostname) { hostname_ = hostname; }

void ArduinoOTAClass::setPassword(const char* password) { password_ = password; }

void ArduinoOTAClass::onStart(std::function<void()> callback) { onStart_ = std::move(callback); }

void ArduinoOTAClass::onEnd(std::function<void()> callback) { onEnd_ = std::move(callback); }

void ArduinoOTAClass::onError(std::function<void(ota_error_t)> callback) {
  onError_ = std::move(callback);
}

void ArduinoOTAClass::begin() { started_ = true; }

void ArduinoOTAClass::handle() {}

void ArduinoOTAClass::triggerStartForTest() {
  if (onStart_) {
    onStart_();
  }
}

void ArduinoOTAClass::triggerEndForTest() {
  if (onEnd_) {
    onEnd_();
  }
}

void ArduinoOTAClass::triggerErrorForTest(ota_error_t error) {
  if (onError_) {
    onError_(error);
  }
}

bool ArduinoOTAClass::startedForTest() const { return started_; }

String ArduinoOTAClass::hostnameForTest() const { return hostname_; }

String ArduinoOTAClass::passwordForTest() const { return password_; }

void ArduinoOTAClass::resetForTest() {
  hostname_.remove(0);
  password_.remove(0);
  started_ = false;
  onStart_ = nullptr;
  onEnd_ = nullptr;
  onError_ = nullptr;
}

bool MDNSClass::begin(const char* hostname) {
  active_ = true;
  hostname_ = hostname;
  return true;
}

void MDNSClass::addService(const char*, const char*, uint16_t) {}

void MDNSClass::end() {
  active_ = false;
  hostname_.remove(0);
}

bool MDNSClass::activeForTest() const { return active_; }

String MDNSClass::hostnameForTest() const { return hostname_; }

void MDNSClass::resetForTest() {
  active_ = false;
  hostname_.remove(0);
}
