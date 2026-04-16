#pragma once

#include <Arduino.h>

#include "app_constants.h"
#include "config_store.h"
#include "time_service.h"

struct PlantStatusSnapshot {
  PlantSettings settings;
  int lastRawReading = -1;
  int lastMoisturePercent = -1;
  bool calibrationValid = false;
  bool pumpRunning = false;
  bool otaLockActive = false;
  uint32_t cooldownRemainingMs = 0;
  WateringEvent wateringHistory[kWateringHistoryLimit];
  size_t wateringHistoryCount = 0;
};

class PlantController {
 public:
  PlantController(ConfigStore& configStore, TimeService& timeService);

  void begin();
  void tick(unsigned long now);

  void readNow(Stream* out = nullptr);
  void captureCalibrationPoint(bool captureDry, Stream* out = nullptr);
  void clearCalibration(Stream* out = nullptr);

  bool setAutoMode(bool enabled, String* errorOut = nullptr);
  void setDryThresholdPercent(uint8_t value);
  void setPumpPulseMs(uint32_t value);
  void setCooldownMs(uint32_t value);
  void setSampleIntervalMs(uint32_t value);

  bool runManualPumpPulse(Stream* out = nullptr);
  void runDiagnosticSweep(Stream& out);
  void enterOtaLock(Stream* out = nullptr);

  PlantStatusSnapshot snapshot(unsigned long now) const;
  void printStatus(Stream& out, unsigned long now) const;

 private:
  bool calibrationIsValid() const;
  int readMoistureRaw();
  int computeMoisturePercent(int rawReading) const;
  void captureMoistureSnapshot();
  void savePersistentConfig();
  void applyPumpOutput(bool enabled);
  void maybeStopPump(unsigned long now);
  void maybeSampleAndWater(unsigned long now);
  void loadWateringHistory();
  void appendWateringEvent(const WateringEvent& event);
  void printReadingLine(Stream& out, const char* label) const;
  void printCalibrationSummary(Stream& out) const;
  bool startPumpPulse(const char* reason, Stream* out = nullptr);
  void stopPump(const char* reason);

  ConfigStore& configStore_;
  TimeService& timeService_;
  PlantSettings settings_;
  int lastRawReading_ = -1;
  int lastMoisturePercent_ = -1;
  unsigned long lastSensorSampleAtMs_ = 0;
  unsigned long pumpStopAtMs_ = 0;
  unsigned long cooldownUntilMs_ = 0;
  bool pumpRunning_ = false;
  bool otaLockActive_ = false;
  WateringEvent wateringHistory_[kWateringHistoryLimit];
  size_t wateringHistoryCount_ = 0;
  uint32_t nextWateringSequence_ = 1;
  WateringReason activeWateringReason_ = WateringReason::Unknown;
  uint64_t activeWateringStartedAtEpochMs_ = 0;
  int activeWateringStartRaw_ = -1;
  int activeWateringStartPercent_ = -1;
  unsigned long activeWateringStartedAtMs_ = 0;
};
