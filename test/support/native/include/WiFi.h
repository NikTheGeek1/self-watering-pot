#pragma once

#include "Arduino.h"

enum wl_status_t : uint8_t {
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6,
};

enum wifi_mode_t : uint8_t {
  WIFI_OFF = 0,
  WIFI_STA = 1,
  WIFI_AP = 2,
};

class WiFiClass {
 public:
  void persistent(bool enabled);
  void setAutoReconnect(bool enabled);
  void mode(wifi_mode_t mode);
  wifi_mode_t mode() const;

  void begin(const char* ssid, const char* password);
  void disconnect(bool wifioff = false, bool eraseap = false);
  bool isConnected() const;
  wl_status_t status() const;

  String SSID() const;
  IPAddress localIP() const;
  void setHostname(const char* hostname);

  bool softAP(const char* ssid, const char* password);
  void softAPdisconnect(bool wifioff = false);
  IPAddress softAPIP() const;

  void setStatusForTest(wl_status_t status);
  void setLocalIpForTest(const IPAddress& ipAddress);
  String stationSsidForTest() const;
  String softApSsidForTest() const;
  String softApPasswordForTest() const;
  String hostnameForTest() const;
  void resetForTest();

 private:
  bool persistent_ = false;
  bool autoReconnect_ = false;
  wifi_mode_t mode_ = WIFI_OFF;
  wl_status_t status_ = WL_DISCONNECTED;
  String stationSsid_;
  String stationPassword_;
  String softApSsid_;
  String softApPassword_;
  String hostname_;
  IPAddress localIp_ = IPAddress(0, 0, 0, 0);
  IPAddress softApIp_ = IPAddress(192, 168, 4, 1);
};

extern WiFiClass WiFi;
