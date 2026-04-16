#pragma once

#include "Arduino.h"

class DNSServer {
 public:
  void start(uint16_t port, const char* domainName, const IPAddress& resolvedIp);
  void stop();
  void processNextRequest();

  bool activeForTest() const;

 private:
  bool active_ = false;
};
