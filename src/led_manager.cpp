#include "led_manager.h"

#include "app_constants.h"

void LedManager::begin() {
  pinMode(kRedLedPin, OUTPUT);
  pinMode(kGreenLedPin, OUTPUT);
  digitalWrite(kRedLedPin, LOW);
  digitalWrite(kGreenLedPin, LOW);
}

void LedManager::tick(unsigned long now, WiFiState wifiState, bool pumpRunning, bool showError) {
  bool greenOn = false;
  switch (wifiState) {
    case WiFiState::SetupAp:
      greenOn = isDoubleBlink(now, 2000);
      break;
    case WiFiState::StaConnecting:
    case WiFiState::StaError:
      greenOn = (now % 500UL) < 250UL;
      break;
    case WiFiState::StaConnected:
      greenOn = (now % kHeartbeatPeriodMs) < kHeartbeatOnMs;
      break;
    case WiFiState::OtaInProgress:
      greenOn = (now % 200UL) < 100UL;
      break;
    case WiFiState::NoCredentials:
      greenOn = false;
      break;
  }

  bool redOn = false;
  if (pumpRunning) {
    redOn = true;
  } else if (showError) {
    redOn = isDoubleBlink(now, 2000);
  }

  digitalWrite(kGreenLedPin, greenOn ? HIGH : LOW);
  digitalWrite(kRedLedPin, redOn ? HIGH : LOW);
}

bool LedManager::isDoubleBlink(unsigned long now, unsigned long cycleMs) const {
  const unsigned long phase = now % cycleMs;
  return (phase < 120UL) || (phase >= 240UL && phase < 360UL);
}
