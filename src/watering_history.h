#pragma once

#include <Arduino.h>

enum class WateringReason : uint8_t {
  Unknown = 0,
  Manual = 1,
  Auto = 2,
};

struct WateringEvent {
  uint32_t sequence = 0;
  WateringReason reason = WateringReason::Unknown;
  uint64_t startedAtEpochMs = 0;
  uint64_t endedAtEpochMs = 0;
  uint32_t durationMs = 0;
  int startRaw = -1;
  int startPercent = -1;
  int endRaw = -1;
  int endPercent = -1;
};

inline const char* wateringReasonToText(WateringReason reason) {
  switch (reason) {
    case WateringReason::Manual:
      return "manual";
    case WateringReason::Auto:
      return "auto";
    case WateringReason::Unknown:
    default:
      return "unknown";
  }
}
