#pragma once

#include <time.h>

#include <Arduino.h>

struct TimeServiceOps {
  void (*configure)(const char* primary, const char* secondary, const char* tertiary);
  time_t (*readSeconds)();
  bool (*readTimespec)(struct timespec* out);
};

class TimeService {
 public:
  explicit TimeService(const TimeServiceOps* ops = nullptr);

  void begin();
  void startSync();
  void tick(unsigned long now, bool wifiConnected);

  bool isSynchronized() const;
  uint64_t currentEpochMs() const;

 private:
  bool looksSynchronized() const;

  const TimeServiceOps* ops_ = nullptr;
  bool syncRequested_ = false;
  bool synchronized_ = false;
  unsigned long nextSyncCheckAtMs_ = 0;
};
