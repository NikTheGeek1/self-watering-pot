#pragma once

#include "Arduino.h"

class MDNSClass {
 public:
  bool begin(const char* hostname);
  void addService(const char* service, const char* protocol, uint16_t port);
  void end();

  bool activeForTest() const;
  String hostnameForTest() const;
  void resetForTest();

 private:
  bool active_ = false;
  String hostname_;
};

extern MDNSClass MDNS;
