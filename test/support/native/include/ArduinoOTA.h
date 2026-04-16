#pragma once

#include <functional>

#include "Arduino.h"

enum ota_error_t : uint8_t {
  OTA_AUTH_ERROR = 0,
  OTA_BEGIN_ERROR = 1,
  OTA_CONNECT_ERROR = 2,
  OTA_RECEIVE_ERROR = 3,
  OTA_END_ERROR = 4,
};

class ArduinoOTAClass {
 public:
  void setHostname(const char* hostname);
  void setPassword(const char* password);

  void onStart(std::function<void()> callback);
  void onEnd(std::function<void()> callback);
  void onError(std::function<void(ota_error_t)> callback);

  void begin();
  void handle();

  void triggerStartForTest();
  void triggerEndForTest();
  void triggerErrorForTest(ota_error_t error);
  bool startedForTest() const;
  String hostnameForTest() const;
  String passwordForTest() const;
  void resetForTest();

 private:
  String hostname_;
  String password_;
  bool started_ = false;
  std::function<void()> onStart_;
  std::function<void()> onEnd_;
  std::function<void(ota_error_t)> onError_;
};

extern ArduinoOTAClass ArduinoOTA;
