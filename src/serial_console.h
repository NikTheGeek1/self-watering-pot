#pragma once

#include <Arduino.h>

#include "app_constants.h"
#include "network_manager.h"
#include "plant_controller.h"

class SerialConsole {
 public:
  SerialConsole(PlantController& plantController, NetworkManager& networkManager);

  void begin();
  void tick();

 private:
  static void trimInPlace(char* text);
  static void toLowerCaseInPlace(char* text);
  static bool startsWith(const char* text, const char* prefix);
  static bool parseUnsignedLongArgument(const char* text, const char* prefix,
                                        unsigned long* valueOut);

  void printHelp() const;
  void printBootSummary() const;
  void printCombinedStatus() const;
  void handleSettingCommand(const char* command);
  void processCommand(char* command);

  PlantController& plantController_;
  NetworkManager& networkManager_;
  char commandBuffer_[kCommandBufferSize] = {};
  size_t commandLength_ = 0;
};
