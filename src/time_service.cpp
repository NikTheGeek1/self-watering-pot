#include "time_service.h"

#include <time.h>

#include "app_constants.h"

namespace {

constexpr time_t kValidEpochThreshold = 1704067200;  // 2024-01-01T00:00:00Z

}  // namespace

void TimeService::begin() {
  syncRequested_ = false;
  synchronized_ = looksSynchronized();
  nextSyncCheckAtMs_ = 0;
}

void TimeService::startSync() {
  configTime(0, 0, kNtpServerPrimary, kNtpServerSecondary, kNtpServerTertiary);
  syncRequested_ = true;
  nextSyncCheckAtMs_ = 0;
}

void TimeService::tick(unsigned long now, bool wifiConnected) {
  if (!wifiConnected) {
    return;
  }

  if (!syncRequested_) {
    startSync();
  }

  if (synchronized_) {
    return;
  }

  if (now < nextSyncCheckAtMs_) {
    return;
  }

  synchronized_ = looksSynchronized();
  if (synchronized_) {
    Serial.println(F("NTP time synchronized"));
    return;
  }

  nextSyncCheckAtMs_ = now + kNtpSyncCheckIntervalMs;
}

bool TimeService::isSynchronized() const { return synchronized_; }

uint64_t TimeService::currentEpochMs() const {
  struct timespec now;
  if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
    return 0;
  }

  if (now.tv_sec < kValidEpochThreshold) {
    return 0;
  }

  return (static_cast<uint64_t>(now.tv_sec) * 1000ULL) +
         (static_cast<uint64_t>(now.tv_nsec) / 1000000ULL);
}

bool TimeService::looksSynchronized() const {
  time_t now = time(nullptr);
  return now >= kValidEpochThreshold;
}
