#include "serial_console.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "app_constants.h"

SerialConsole::SerialConsole(PlantController& plantController, NetworkManager& networkManager)
    : plantController_(plantController), networkManager_(networkManager) {}

void SerialConsole::begin() { printBootSummary(); }

void SerialConsole::tick() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());

    if (incoming == '\r' || incoming == '\n') {
      if (commandLength_ > 0) {
        commandBuffer_[commandLength_] = '\0';
        processCommand(commandBuffer_);
        commandLength_ = 0;
      }
      continue;
    }

    if (commandLength_ < kCommandBufferSize - 1) {
      commandBuffer_[commandLength_++] = incoming;
    }
  }
}

void SerialConsole::trimInPlace(char* text) {
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

void SerialConsole::toLowerCaseInPlace(char* text) {
  for (char* cursor = text; *cursor != '\0'; ++cursor) {
    *cursor = static_cast<char>(tolower(static_cast<unsigned char>(*cursor)));
  }
}

bool SerialConsole::startsWith(const char* text, const char* prefix) {
  return strncmp(text, prefix, strlen(prefix)) == 0;
}

bool SerialConsole::parseUnsignedLongArgument(const char* text, const char* prefix,
                                              unsigned long* valueOut) {
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

void SerialConsole::printHelp() const {
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("  h | ? | help        -> show help"));
  Serial.println(F("  m | read            -> read moisture now"));
  Serial.println(F("  s | status          -> print plant and network status"));
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
  Serial.println(F("  wifi clear          -> clear saved Wi-Fi credentials and reopen setup AP"));
  Serial.println();
}

void SerialConsole::printBootSummary() const {
  Serial.println();
  Serial.println(F("Smart Pot controller boot"));
  Serial.print(F("Firmware version: "));
  Serial.println(kAppVersion);
  Serial.println(F("Pins: green=13 red=14 pump=26 moisture=34"));
  Serial.println(F("Auto mode starts disabled on every boot for safety."));
  printCombinedStatus();
  printHelp();
}

void SerialConsole::printCombinedStatus() const {
  plantController_.printStatus(Serial, millis());
  networkManager_.printStatus(Serial);
}

void SerialConsole::handleSettingCommand(const char* command) {
  unsigned long parsedValue = 0;

  if (parseUnsignedLongArgument(command, "set threshold ", &parsedValue)) {
    plantController_.setDryThresholdPercent(static_cast<uint8_t>(parsedValue));
    const PlantStatusSnapshot status = plantController_.snapshot(millis());
    Serial.print(F("Dry threshold set to "));
    Serial.print(status.settings.dryThresholdPercent);
    Serial.println(F("%"));
    return;
  }

  if (parseUnsignedLongArgument(command, "set pulse ", &parsedValue)) {
    plantController_.setPumpPulseMs(static_cast<uint32_t>(parsedValue));
    const PlantStatusSnapshot status = plantController_.snapshot(millis());
    Serial.print(F("Pump pulse set to "));
    Serial.print(status.settings.pumpPulseMs);
    Serial.println(F(" ms"));
    return;
  }

  if (parseUnsignedLongArgument(command, "set cooldown ", &parsedValue)) {
    plantController_.setCooldownMs(static_cast<uint32_t>(parsedValue));
    const PlantStatusSnapshot status = plantController_.snapshot(millis());
    Serial.print(F("Cooldown set to "));
    Serial.print(status.settings.cooldownMs);
    Serial.println(F(" ms"));
    return;
  }

  if (parseUnsignedLongArgument(command, "set sample ", &parsedValue)) {
    plantController_.setSampleIntervalMs(static_cast<uint32_t>(parsedValue));
    const PlantStatusSnapshot status = plantController_.snapshot(millis());
    Serial.print(F("Sample interval set to "));
    Serial.print(status.settings.sampleIntervalMs);
    Serial.println(F(" ms"));
    return;
  }

  Serial.println(F("Unknown set command"));
}

void SerialConsole::processCommand(char* command) {
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
    plantController_.readNow(&Serial);
    return;
  }

  if (strcmp(command, "s") == 0 || strcmp(command, "status") == 0) {
    printCombinedStatus();
    return;
  }

  if (strcmp(command, "p") == 0 || strcmp(command, "pump") == 0) {
    plantController_.runManualPumpPulse(&Serial);
    return;
  }

  if (strcmp(command, "d") == 0 || strcmp(command, "diag") == 0) {
    plantController_.runDiagnosticSweep(Serial);
    return;
  }

  if (strcmp(command, "auto on") == 0) {
    String error;
    if (plantController_.setAutoMode(true, &error)) {
      Serial.println(F("Auto mode enabled"));
    } else {
      Serial.println(error);
    }
    return;
  }

  if (strcmp(command, "auto off") == 0) {
    plantController_.setAutoMode(false);
    Serial.println(F("Auto mode disabled"));
    return;
  }

  if (strcmp(command, "cal dry") == 0) {
    plantController_.captureCalibrationPoint(true, &Serial);
    return;
  }

  if (strcmp(command, "cal wet") == 0) {
    plantController_.captureCalibrationPoint(false, &Serial);
    return;
  }

  if (strcmp(command, "cal clear") == 0) {
    plantController_.clearCalibration(&Serial);
    return;
  }

  if (strcmp(command, "wifi clear") == 0) {
    networkManager_.clearCredentialsAndEnterSetupMode();
    Serial.println(F("Wi-Fi credentials cleared. Setup AP is active at http://192.168.4.1"));
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
