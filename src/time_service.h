#pragma once

#include <Arduino.h>

class TimeService {
 public:
  void begin();
  void startSync();
  void tick(unsigned long now, bool wifiConnected);

  bool isSynchronized() const;
  uint64_t currentEpochMs() const;

 private:
  bool looksSynchronized() const;

  bool syncRequested_ = false;
  bool synchronized_ = false;
  unsigned long nextSyncCheckAtMs_ = 0;
};
