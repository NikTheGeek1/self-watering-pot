#include "plant_controller.h"

#include "app_constants.h"

namespace {

uint32_t clampUint32(uint32_t value, uint32_t minimumValue, uint32_t maximumValue) {
  if (value < minimumValue) {
    return minimumValue;
  }

  if (value > maximumValue) {
    return maximumValue;
  }

  return value;
}

WateringReason wateringReasonFromLabel(const char* reason) {
  if (strcmp(reason, "manual") == 0) {
    return WateringReason::Manual;
  }

  if (strcmp(reason, "auto") == 0) {
    return WateringReason::Auto;
  }

  return WateringReason::Unknown;
}

}  // namespace

PlantController::PlantController(ConfigStore& configStore, TimeService& timeService)
    : configStore_(configStore), timeService_(timeService) {}

void PlantController::begin() {
  pinMode(kPumpPin, OUTPUT);
  digitalWrite(kPumpPin, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(kMoisturePin, ADC_11db);

  settings_ = configStore_.loadPlantSettings();
  loadWateringHistory();
  captureMoistureSnapshot();
  lastSensorSampleAtMs_ = millis();
}

void PlantController::tick(unsigned long now) {
  maybeStopPump(now);
  maybeSampleAndWater(now);
}

void PlantController::readNow(Stream* out) {
  captureMoistureSnapshot();
  if (out != nullptr) {
    printReadingLine(*out, "Manual reading");
  }
}

void PlantController::captureCalibrationPoint(bool captureDry, Stream* out) {
  captureMoistureSnapshot();

  if (captureDry) {
    settings_.dryRaw = lastRawReading_;
  } else {
    settings_.wetRaw = lastRawReading_;
  }

  savePersistentConfig();

  if (out != nullptr) {
    out->print(captureDry ? F("Saved dry calibration from raw=")
                          : F("Saved wet calibration from raw="));
    out->println(lastRawReading_);
    printCalibrationSummary(*out);
  }
}

void PlantController::clearCalibration(Stream* out) {
  settings_.dryRaw = -1;
  settings_.wetRaw = -1;
  settings_.autoEnabled = false;
  savePersistentConfig();

  if (out != nullptr) {
    out->println(F("Calibration cleared and auto mode disabled"));
  }
}

bool PlantController::setAutoMode(bool enabled, String* errorOut) {
  if (otaLockActive_ && enabled) {
    if (errorOut != nullptr) {
      *errorOut = F("Auto mode is locked until reboot because an OTA session started.");
    }
    return false;
  }

  if (enabled && !calibrationIsValid()) {
    if (errorOut != nullptr) {
      *errorOut = F("Auto mode requires both dry and wet calibration values.");
    }
    return false;
  }

  settings_.autoEnabled = enabled;
  return true;
}

void PlantController::setDryThresholdPercent(uint8_t value) {
  settings_.dryThresholdPercent = static_cast<uint8_t>(
      clampUint32(value, kMinThresholdPercent, kMaxThresholdPercent));
  savePersistentConfig();
}

void PlantController::setPumpPulseMs(uint32_t value) {
  settings_.pumpPulseMs = clampUint32(value, kMinPumpPulseMs, kMaxPumpPulseMs);
  savePersistentConfig();
}

void PlantController::setCooldownMs(uint32_t value) {
  settings_.cooldownMs = clampUint32(value, kMinCooldownMs, kMaxCooldownMs);
  savePersistentConfig();
}

void PlantController::setSampleIntervalMs(uint32_t value) {
  settings_.sampleIntervalMs = clampUint32(value, kMinSampleIntervalMs, kMaxSampleIntervalMs);
  savePersistentConfig();
}

bool PlantController::runManualPumpPulse(Stream* out) {
  return startPumpPulse("manual", out);
}

void PlantController::runDiagnosticSweep(Stream& out) {
  if (otaLockActive_) {
    out.println(F("Diagnostic sweep skipped: OTA lock is active until reboot"));
    return;
  }

  if (pumpRunning_) {
    out.println(F("Diagnostic sweep skipped: pump already running"));
    return;
  }

  out.println(F("Diagnostic sweep start: GPIO26 HIGH for 2000 ms, then LOW for 2000 ms"));
  digitalWrite(kPumpPin, HIGH);
  delay(kDiagnosticStepMs);
  digitalWrite(kPumpPin, LOW);
  delay(kDiagnosticStepMs);
  out.println(F("Diagnostic sweep end; GPIO26 returned LOW"));
}

void PlantController::enterOtaLock(Stream* out) {
  if (otaLockActive_) {
    return;
  }

  otaLockActive_ = true;
  settings_.autoEnabled = false;

  if (pumpRunning_) {
    stopPump("ota");
  }

  if (out != nullptr) {
    out->println(F("OTA lock enabled: automatic watering and new pump actions stay disabled until reboot"));
  }
}

PlantStatusSnapshot PlantController::snapshot(unsigned long now) const {
  PlantStatusSnapshot status;
  status.settings = settings_;
  status.lastRawReading = lastRawReading_;
  status.lastMoisturePercent = lastMoisturePercent_;
  status.calibrationValid = calibrationIsValid();
  status.pumpRunning = pumpRunning_;
  status.otaLockActive = otaLockActive_;
  status.cooldownRemainingMs = (!pumpRunning_ && cooldownUntilMs_ > now)
                                   ? static_cast<uint32_t>(cooldownUntilMs_ - now)
                                   : 0;
  status.wateringHistoryCount = wateringHistoryCount_;
  for (size_t i = 0; i < wateringHistoryCount_; ++i) {
    status.wateringHistory[i] = wateringHistory_[i];
  }
  return status;
}

void PlantController::printStatus(Stream& out, unsigned long now) const {
  const PlantStatusSnapshot status = snapshot(now);

  out.println();
  out.println(F("Plant Status"));
  out.print(F("  Auto mode: "));
  out.println(status.settings.autoEnabled ? F("ON") : F("OFF"));
  out.print(F("  Pump running: "));
  out.println(status.pumpRunning ? F("YES") : F("NO"));
  out.print(F("  OTA lock: "));
  out.println(status.otaLockActive ? F("YES") : F("NO"));
  out.print(F("  Dry threshold: "));
  out.print(status.settings.dryThresholdPercent);
  out.println(F("%"));
  out.print(F("  Pump pulse: "));
  out.print(status.settings.pumpPulseMs);
  out.println(F(" ms"));
  out.print(F("  Cooldown: "));
  out.print(status.settings.cooldownMs);
  out.println(F(" ms"));
  out.print(F("  Sample interval: "));
  out.print(status.settings.sampleIntervalMs);
  out.println(F(" ms"));
  printCalibrationSummary(out);

  if (status.lastRawReading >= 0) {
    printReadingLine(out, "  Last reading");
  }

  if (status.cooldownRemainingMs > 0) {
    out.print(F("  Cooldown remaining: "));
    out.print(status.cooldownRemainingMs / 1000UL);
    out.println(F(" s"));
  }

  out.println();
}

bool PlantController::calibrationIsValid() const {
  return settings_.dryRaw >= 0 && settings_.wetRaw >= 0 && settings_.dryRaw != settings_.wetRaw;
}

int PlantController::readMoistureRaw() {
  uint32_t total = 0;
  for (size_t i = 0; i < kMoistureSamplesPerRead; ++i) {
    total += analogRead(kMoisturePin);
    delay(4);
  }

  return static_cast<int>(total / kMoistureSamplesPerRead);
}

int PlantController::computeMoisturePercent(int rawReading) const {
  if (!calibrationIsValid()) {
    return -1;
  }

  const float numerator = static_cast<float>(rawReading - settings_.dryRaw);
  const float denominator = static_cast<float>(settings_.wetRaw - settings_.dryRaw);
  const int percent = static_cast<int>(lroundf((numerator / denominator) * 100.0f));
  return constrain(percent, 0, 100);
}

void PlantController::captureMoistureSnapshot() {
  lastRawReading_ = readMoistureRaw();
  lastMoisturePercent_ = computeMoisturePercent(lastRawReading_);
}

void PlantController::savePersistentConfig() { configStore_.savePlantSettings(settings_); }

void PlantController::applyPumpOutput(bool enabled) { digitalWrite(kPumpPin, enabled ? HIGH : LOW); }

void PlantController::loadWateringHistory() {
  wateringHistoryCount_ = configStore_.loadWateringHistory(wateringHistory_, kWateringHistoryLimit);
  nextWateringSequence_ = 1;

  for (size_t i = 0; i < wateringHistoryCount_; ++i) {
    if (wateringHistory_[i].sequence >= nextWateringSequence_) {
      nextWateringSequence_ = wateringHistory_[i].sequence + 1;
    }
  }
}

void PlantController::appendWateringEvent(const WateringEvent& event) {
  const size_t shiftCount =
      wateringHistoryCount_ < kWateringHistoryLimit ? wateringHistoryCount_ : kWateringHistoryLimit - 1;

  for (size_t i = shiftCount; i > 0; --i) {
    wateringHistory_[i] = wateringHistory_[i - 1];
  }

  wateringHistory_[0] = event;
  if (wateringHistoryCount_ < kWateringHistoryLimit) {
    ++wateringHistoryCount_;
  }

  configStore_.saveWateringHistory(wateringHistory_, wateringHistoryCount_);
}

void PlantController::maybeStopPump(unsigned long now) {
  if (!pumpRunning_ || now < pumpStopAtMs_) {
    return;
  }

  stopPump("completed");
}

void PlantController::maybeSampleAndWater(unsigned long now) {
  if (now - lastSensorSampleAtMs_ < settings_.sampleIntervalMs) {
    return;
  }

  lastSensorSampleAtMs_ = now;
  captureMoistureSnapshot();
  printReadingLine(Serial, "Periodic reading");

  if (!settings_.autoEnabled || pumpRunning_ || now < cooldownUntilMs_ || otaLockActive_) {
    return;
  }

  if (!calibrationIsValid()) {
    Serial.println(F("Auto mode suspended: calibration incomplete"));
    settings_.autoEnabled = false;
    return;
  }

  if (lastMoisturePercent_ > settings_.dryThresholdPercent) {
    return;
  }

  Serial.print(F("Auto watering triggered at "));
  Serial.print(lastMoisturePercent_);
  Serial.print(F("% <= threshold "));
  Serial.print(settings_.dryThresholdPercent);
  Serial.println(F("%"));
  startPumpPulse("auto", &Serial);
}

void PlantController::printReadingLine(Stream& out, const char* label) const {
  out.print(label);
  out.print(F(": raw="));
  out.print(lastRawReading_);

  if (lastMoisturePercent_ >= 0) {
    out.print(F(" moisture="));
    out.print(lastMoisturePercent_);
    out.print(F("%"));
  } else {
    out.print(F(" moisture=uncalibrated"));
  }

  out.println();
}

void PlantController::printCalibrationSummary(Stream& out) const {
  out.print(F("  Calibration: dry="));
  if (settings_.dryRaw >= 0) {
    out.print(settings_.dryRaw);
  } else {
    out.print(F("unset"));
  }

  out.print(F(" wet="));
  if (settings_.wetRaw >= 0) {
    out.print(settings_.wetRaw);
  } else {
    out.print(F("unset"));
  }

  out.println(calibrationIsValid() ? F(" (valid)") : F(" (needs both dry and wet values)"));
}

bool PlantController::startPumpPulse(const char* reason, Stream* out) {
  if (otaLockActive_) {
    if (out != nullptr) {
      out->println(F("Pump request ignored: OTA lock is active until reboot"));
    }
    return false;
  }

  if (pumpRunning_) {
    if (out != nullptr) {
      out->println(F("Pump request ignored: pump already running"));
    }
    return false;
  }

  captureMoistureSnapshot();
  activeWateringReason_ = wateringReasonFromLabel(reason);
  activeWateringStartedAtEpochMs_ = timeService_.currentEpochMs();
  activeWateringStartRaw_ = lastRawReading_;
  activeWateringStartPercent_ = lastMoisturePercent_;
  activeWateringStartedAtMs_ = millis();

  applyPumpOutput(true);
  pumpRunning_ = true;
  pumpStopAtMs_ = millis() + settings_.pumpPulseMs;

  if (out != nullptr) {
    out->print(F("Pump start ("));
    out->print(reason);
    out->print(F(") for "));
    out->print(settings_.pumpPulseMs);
    out->println(F(" ms"));
  }

  return true;
}

void PlantController::stopPump(const char* reason) {
  const unsigned long stoppedAtMs = millis();
  applyPumpOutput(false);
  pumpRunning_ = false;
  cooldownUntilMs_ = stoppedAtMs + settings_.cooldownMs;
  captureMoistureSnapshot();

  WateringEvent event;
  event.sequence = nextWateringSequence_++;
  event.reason = activeWateringReason_;
  event.startedAtEpochMs = activeWateringStartedAtEpochMs_;
  event.endedAtEpochMs = timeService_.currentEpochMs();
  event.durationMs = stoppedAtMs - activeWateringStartedAtMs_;
  event.startRaw = activeWateringStartRaw_;
  event.startPercent = activeWateringStartPercent_;
  event.endRaw = lastRawReading_;
  event.endPercent = lastMoisturePercent_;
  appendWateringEvent(event);

  activeWateringReason_ = WateringReason::Unknown;
  activeWateringStartedAtEpochMs_ = 0;
  activeWateringStartRaw_ = -1;
  activeWateringStartPercent_ = -1;
  activeWateringStartedAtMs_ = 0;

  Serial.print(F("Pump stop ("));
  Serial.print(reason);
  Serial.print(F(") duration="));
  Serial.print(event.durationMs);
  Serial.print(F(" ms start="));
  if (event.startPercent >= 0) {
    Serial.print(event.startPercent);
    Serial.print(F("%"));
  } else {
    Serial.print(F("uncalibrated"));
  }
  Serial.print(F(" end="));
  if (event.endPercent >= 0) {
    Serial.print(event.endPercent);
    Serial.println(F("%"));
  } else {
    Serial.println(F("uncalibrated"));
  }
}
