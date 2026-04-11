#include <Arduino.h>
#include <Preferences.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint8_t kRedLedPin = 19;
constexpr uint8_t kGreenLedPin = 18;
constexpr uint8_t kPumpPin = 26;
constexpr uint8_t kMoisturePin = 34;

constexpr unsigned long kHeartbeatPeriodMs = 1000;
constexpr unsigned long kHeartbeatOnMs = 150;
constexpr unsigned long kDiagnosticStepMs = 2000;

constexpr uint32_t kDefaultPumpPulseMs = 1200;
constexpr uint32_t kDefaultCooldownMs = 45000;
constexpr uint32_t kDefaultSampleIntervalMs = 5000;
constexpr uint8_t kDefaultDryThresholdPercent = 35;

constexpr uint32_t kMinPumpPulseMs = 250;
constexpr uint32_t kMaxPumpPulseMs = 3000;
constexpr uint32_t kMinCooldownMs = 5000;
constexpr uint32_t kMinSampleIntervalMs = 1000;
constexpr uint8_t kMinThresholdPercent = 5;
constexpr uint8_t kMaxThresholdPercent = 95;

constexpr size_t kCommandBufferSize = 96;
constexpr size_t kMoistureSamplesPerRead = 8;

Preferences preferences;

struct RuntimeConfig {
  int dryRaw = -1;
  int wetRaw = -1;
  uint8_t dryThresholdPercent = kDefaultDryThresholdPercent;
  uint32_t pumpPulseMs = kDefaultPumpPulseMs;
  uint32_t cooldownMs = kDefaultCooldownMs;
  uint32_t sampleIntervalMs = kDefaultSampleIntervalMs;
  bool autoEnabled = false;
};

RuntimeConfig config;

char commandBuffer[kCommandBufferSize];
size_t commandLength = 0;

int lastRawReading = -1;
int lastMoisturePercent = -1;

unsigned long lastSensorSampleAtMs = 0;
unsigned long pumpStopAtMs = 0;
unsigned long cooldownUntilMs = 0;

bool pumpRunning = false;

bool calibrationIsValid() {
  return config.dryRaw >= 0 && config.wetRaw >= 0 && config.dryRaw != config.wetRaw;
}

uint32_t clampUint32(uint32_t value, uint32_t minimumValue, uint32_t maximumValue) {
  if (value < minimumValue) {
    return minimumValue;
  }

  if (value > maximumValue) {
    return maximumValue;
  }

  return value;
}

void trimInPlace(char* text) {
  size_t length = strlen(text);
  size_t start = 0;
  while (start < length && isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }

  size_t end = length;
  while (end > start && isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }

  if (start > 0) {
    memmove(text, text + start, end - start);
  }

  text[end - start] = '\0';
}

void toLowerCaseInPlace(char* text) {
  for (char* cursor = text; *cursor != '\0'; ++cursor) {
    *cursor = static_cast<char>(tolower(static_cast<unsigned char>(*cursor)));
  }
}

bool startsWith(const char* text, const char* prefix) {
  return strncmp(text, prefix, strlen(prefix)) == 0;
}

bool parseUnsignedLongArgument(const char* text, const char* prefix, unsigned long* valueOut) {
  if (!startsWith(text, prefix)) {
    return false;
  }

  const char* valueText = text + strlen(prefix);
  if (*valueText == '\0') {
    return false;
  }

  char* endPtr = nullptr;
  const unsigned long parsed = strtoul(valueText, &endPtr, 10);
  if (endPtr == valueText) {
    return false;
  }

  while (*endPtr != '\0' && isspace(static_cast<unsigned char>(*endPtr)) != 0) {
    ++endPtr;
  }

  if (*endPtr != '\0') {
    return false;
  }

  *valueOut = parsed;
  return true;
}

void applyPumpOutput(bool enabled) {
  digitalWrite(kPumpPin, enabled ? HIGH : LOW);
  digitalWrite(kRedLedPin, enabled ? HIGH : LOW);
}

void savePersistentConfig() {
  preferences.putInt("dry_raw", config.dryRaw);
  preferences.putInt("wet_raw", config.wetRaw);
  preferences.putUChar("threshold", config.dryThresholdPercent);
  preferences.putUInt("pulse_ms", config.pumpPulseMs);
  preferences.putUInt("cool_ms", config.cooldownMs);
  preferences.putUInt("sample_ms", config.sampleIntervalMs);
}

void loadPersistentConfig() {
  preferences.begin("smart-plant", false);

  config.dryRaw = preferences.getInt("dry_raw", -1);
  config.wetRaw = preferences.getInt("wet_raw", -1);
  config.dryThresholdPercent =
      preferences.getUChar("threshold", kDefaultDryThresholdPercent);
  config.pumpPulseMs =
      clampUint32(preferences.getUInt("pulse_ms", kDefaultPumpPulseMs), kMinPumpPulseMs,
                  kMaxPumpPulseMs);
  config.cooldownMs =
      clampUint32(preferences.getUInt("cool_ms", kDefaultCooldownMs), kMinCooldownMs,
                  10UL * 60UL * 1000UL);
  config.sampleIntervalMs =
      clampUint32(preferences.getUInt("sample_ms", kDefaultSampleIntervalMs),
                  kMinSampleIntervalMs, 10UL * 60UL * 1000UL);
  config.dryThresholdPercent = static_cast<uint8_t>(
      clampUint32(config.dryThresholdPercent, kMinThresholdPercent, kMaxThresholdPercent));
  config.autoEnabled = false;
}

int readMoistureRaw() {
  uint32_t total = 0;
  for (size_t i = 0; i < kMoistureSamplesPerRead; ++i) {
    total += analogRead(kMoisturePin);
    delay(4);
  }

  return static_cast<int>(total / kMoistureSamplesPerRead);
}

int computeMoisturePercent(int rawReading) {
  if (!calibrationIsValid()) {
    return -1;
  }

  const float numerator = static_cast<float>(rawReading - config.dryRaw);
  const float denominator = static_cast<float>(config.wetRaw - config.dryRaw);
  const int percent = static_cast<int>(lroundf((numerator / denominator) * 100.0f));
  return constrain(percent, 0, 100);
}

void captureMoistureSnapshot() {
  lastRawReading = readMoistureRaw();
  lastMoisturePercent = computeMoisturePercent(lastRawReading);
}

void printReadingLine(const char* label) {
  Serial.print(label);
  Serial.print(F(": raw="));
  Serial.print(lastRawReading);

  if (lastMoisturePercent >= 0) {
    Serial.print(F(" moisture="));
    Serial.print(lastMoisturePercent);
    Serial.print(F("%"));
  } else {
    Serial.print(F(" moisture=uncalibrated"));
  }

  Serial.println();
}

void printCalibrationSummary() {
  Serial.print(F("Calibration: dry="));
  if (config.dryRaw >= 0) {
    Serial.print(config.dryRaw);
  } else {
    Serial.print(F("unset"));
  }

  Serial.print(F(" wet="));
  if (config.wetRaw >= 0) {
    Serial.print(config.wetRaw);
  } else {
    Serial.print(F("unset"));
  }

  if (calibrationIsValid()) {
    Serial.println(F(" (valid)"));
  } else {
    Serial.println(F(" (needs both dry and wet values)"));
  }
}

void printStatus() {
  Serial.println();
  Serial.println(F("Status"));
  Serial.print(F("  Auto mode: "));
  Serial.println(config.autoEnabled ? F("ON") : F("OFF"));
  Serial.print(F("  Pump running: "));
  Serial.println(pumpRunning ? F("YES") : F("NO"));
  Serial.print(F("  Dry threshold: "));
  Serial.print(config.dryThresholdPercent);
  Serial.println(F("%"));
  Serial.print(F("  Pump pulse: "));
  Serial.print(config.pumpPulseMs);
  Serial.println(F(" ms"));
  Serial.print(F("  Cooldown: "));
  Serial.print(config.cooldownMs);
  Serial.println(F(" ms"));
  Serial.print(F("  Sample interval: "));
  Serial.print(config.sampleIntervalMs);
  Serial.println(F(" ms"));
  printCalibrationSummary();

  if (lastRawReading >= 0) {
    printReadingLine("  Last reading");
  }

  const unsigned long now = millis();
  if (!pumpRunning && cooldownUntilMs > now) {
    Serial.print(F("  Cooldown remaining: "));
    Serial.print((cooldownUntilMs - now) / 1000UL);
    Serial.println(F(" s"));
  }

  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("  h | ? | help        -> show help"));
  Serial.println(F("  m | read            -> read moisture now"));
  Serial.println(F("  s | status          -> print status and config"));
  Serial.println(F("  p | pump            -> run one manual pump pulse"));
  Serial.println(F("  d | diag            -> run GPIO26 HIGH then LOW diagnostic sweep"));
  Serial.println(F("  auto on             -> enable automatic watering"));
  Serial.println(F("  auto off            -> disable automatic watering"));
  Serial.println(F("  cal dry             -> save current raw reading as dry calibration"));
  Serial.println(F("  cal wet             -> save current raw reading as wet calibration"));
  Serial.println(F("  cal clear           -> clear stored calibration"));
  Serial.println(F("  set threshold <n>   -> dry threshold percent (5-95)"));
  Serial.println(F("  set pulse <ms>      -> pump pulse duration (250-3000)"));
  Serial.println(F("  set cooldown <ms>   -> minimum wait after watering"));
  Serial.println(F("  set sample <ms>     -> periodic moisture sample interval"));
  Serial.println();
}

void stopPump(const char* reason) {
  applyPumpOutput(false);
  pumpRunning = false;
  cooldownUntilMs = millis() + config.cooldownMs;

  Serial.print(F("Pump stop ("));
  Serial.print(reason);
  Serial.println(F(")"));
}

void startPumpPulse(const char* reason) {
  if (pumpRunning) {
    Serial.println(F("Pump request ignored: pump already running"));
    return;
  }

  applyPumpOutput(true);
  pumpRunning = true;
  pumpStopAtMs = millis() + config.pumpPulseMs;

  Serial.print(F("Pump start ("));
  Serial.print(reason);
  Serial.print(F(") for "));
  Serial.print(config.pumpPulseMs);
  Serial.println(F(" ms"));
}

void captureCalibrationPoint(bool captureDry) {
  captureMoistureSnapshot();

  if (captureDry) {
    config.dryRaw = lastRawReading;
    Serial.print(F("Saved dry calibration from raw="));
  } else {
    config.wetRaw = lastRawReading;
    Serial.print(F("Saved wet calibration from raw="));
  }

  Serial.println(lastRawReading);
  savePersistentConfig();
  printCalibrationSummary();
}

void clearCalibration() {
  config.dryRaw = -1;
  config.wetRaw = -1;
  config.autoEnabled = false;
  savePersistentConfig();
  Serial.println(F("Calibration cleared and auto mode disabled"));
}

void setAutoMode(bool enabled) {
  if (enabled && !calibrationIsValid()) {
    Serial.println(F("Auto mode requires both dry and wet calibration values"));
    return;
  }

  config.autoEnabled = enabled;
  Serial.print(F("Auto mode "));
  Serial.println(config.autoEnabled ? F("enabled") : F("disabled"));
}

void runPumpDiagnosticSweep() {
  if (pumpRunning) {
    Serial.println(F("Diagnostic sweep skipped: pump already running"));
    return;
  }

  Serial.println(F("Diagnostic sweep start: GPIO26 HIGH for 2000 ms, then LOW for 2000 ms"));
  digitalWrite(kRedLedPin, HIGH);
  digitalWrite(kPumpPin, HIGH);
  delay(kDiagnosticStepMs);
  digitalWrite(kPumpPin, LOW);
  delay(kDiagnosticStepMs);
  digitalWrite(kRedLedPin, LOW);
  applyPumpOutput(false);

  Serial.println(F("Diagnostic sweep end; GPIO26 returned LOW"));
}

void handleSettingCommand(const char* command) {
  unsigned long parsedValue = 0;

  if (parseUnsignedLongArgument(command, "set threshold ", &parsedValue)) {
    config.dryThresholdPercent = static_cast<uint8_t>(
        clampUint32(parsedValue, kMinThresholdPercent, kMaxThresholdPercent));
    savePersistentConfig();
    Serial.print(F("Dry threshold set to "));
    Serial.print(config.dryThresholdPercent);
    Serial.println(F("%"));
    return;
  }

  if (parseUnsignedLongArgument(command, "set pulse ", &parsedValue)) {
    config.pumpPulseMs = clampUint32(parsedValue, kMinPumpPulseMs, kMaxPumpPulseMs);
    savePersistentConfig();
    Serial.print(F("Pump pulse set to "));
    Serial.print(config.pumpPulseMs);
    Serial.println(F(" ms"));
    return;
  }

  if (parseUnsignedLongArgument(command, "set cooldown ", &parsedValue)) {
    config.cooldownMs =
        clampUint32(parsedValue, kMinCooldownMs, 10UL * 60UL * 1000UL);
    savePersistentConfig();
    Serial.print(F("Cooldown set to "));
    Serial.print(config.cooldownMs);
    Serial.println(F(" ms"));
    return;
  }

  if (parseUnsignedLongArgument(command, "set sample ", &parsedValue)) {
    config.sampleIntervalMs =
        clampUint32(parsedValue, kMinSampleIntervalMs, 10UL * 60UL * 1000UL);
    savePersistentConfig();
    Serial.print(F("Sample interval set to "));
    Serial.print(config.sampleIntervalMs);
    Serial.println(F(" ms"));
    return;
  }

  Serial.println(F("Unknown set command"));
}

void processCommand(char* command) {
  trimInPlace(command);
  toLowerCaseInPlace(command);

  if (command[0] == '\0') {
    return;
  }

  if (strcmp(command, "h") == 0 || strcmp(command, "?") == 0 ||
      strcmp(command, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(command, "m") == 0 || strcmp(command, "read") == 0) {
    captureMoistureSnapshot();
    printReadingLine("Manual reading");
    return;
  }

  if (strcmp(command, "s") == 0 || strcmp(command, "status") == 0) {
    printStatus();
    return;
  }

  if (strcmp(command, "p") == 0 || strcmp(command, "pump") == 0) {
    startPumpPulse("manual");
    return;
  }

  if (strcmp(command, "d") == 0 || strcmp(command, "diag") == 0) {
    runPumpDiagnosticSweep();
    return;
  }

  if (strcmp(command, "auto on") == 0) {
    setAutoMode(true);
    return;
  }

  if (strcmp(command, "auto off") == 0) {
    setAutoMode(false);
    return;
  }

  if (strcmp(command, "cal dry") == 0) {
    captureCalibrationPoint(true);
    return;
  }

  if (strcmp(command, "cal wet") == 0) {
    captureCalibrationPoint(false);
    return;
  }

  if (strcmp(command, "cal clear") == 0) {
    clearCalibration();
    return;
  }

  if (startsWith(command, "set ")) {
    handleSettingCommand(command);
    return;
  }

  Serial.print(F("Unknown command: "));
  Serial.println(command);
  printHelp();
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r' || incoming == '\n') {
      if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        processCommand(commandBuffer);
        commandLength = 0;
      }
      continue;
    }

    if (commandLength < kCommandBufferSize - 1) {
      commandBuffer[commandLength++] = incoming;
    }
  }
}

void maybeStopPump() {
  if (!pumpRunning) {
    return;
  }

  if (millis() < pumpStopAtMs) {
    return;
  }

  stopPump("completed");
}

void maybeSampleAndWater() {
  const unsigned long now = millis();
  if (pumpRunning) {
    return;
  }

  if (now < cooldownUntilMs) {
    return;
  }

  if (now - lastSensorSampleAtMs < config.sampleIntervalMs) {
    return;
  }

  lastSensorSampleAtMs = now;
  captureMoistureSnapshot();
  printReadingLine("Periodic reading");

  if (!config.autoEnabled) {
    return;
  }

  if (!calibrationIsValid()) {
    Serial.println(F("Auto mode suspended: calibration incomplete"));
    config.autoEnabled = false;
    return;
  }

  if (lastMoisturePercent > config.dryThresholdPercent) {
    return;
  }

  Serial.print(F("Auto watering triggered at "));
  Serial.print(lastMoisturePercent);
  Serial.print(F("% <= threshold "));
  Serial.print(config.dryThresholdPercent);
  Serial.println(F("%"));
  startPumpPulse("auto");
}

void updateHeartbeat() {
  const unsigned long phase = millis() % kHeartbeatPeriodMs;
  digitalWrite(kGreenLedPin, phase < kHeartbeatOnMs ? HIGH : LOW);
}

void printBootSummary() {
  Serial.println();
  Serial.println(F("Smart Plant controller boot"));
  Serial.println(F("Pins: green=18 red=19 pump=26 moisture=34"));
  Serial.println(F("Green heartbeat = healthy idle. Red = pump active."));
  Serial.println(F("Auto mode starts disabled on every boot for safety."));
  printCalibrationSummary();
  printStatus();
  printHelp();
}

}  // namespace

void setup() {
  pinMode(kRedLedPin, OUTPUT);
  pinMode(kGreenLedPin, OUTPUT);
  pinMode(kPumpPin, OUTPUT);

  digitalWrite(kRedLedPin, LOW);
  digitalWrite(kGreenLedPin, LOW);
  applyPumpOutput(false);

  analogReadResolution(12);
  analogSetPinAttenuation(kMoisturePin, ADC_11db);

  loadPersistentConfig();

  Serial.begin(115200);
  delay(500);

  captureMoistureSnapshot();
  lastSensorSampleAtMs = millis();
  printBootSummary();
}

void loop() {
  handleSerialCommands();
  maybeStopPump();
  maybeSampleAndWater();
  updateHeartbeat();
}
