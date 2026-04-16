#pragma once

#include <Arduino.h>

#include "network_manager.h"

class LedManager {
 public:
  void begin();
  void tick(unsigned long now, WiFiState wifiState, bool pumpRunning, bool showError);

 private:
  bool isDoubleBlink(unsigned long now, unsigned long cycleMs) const;
};
