#include <Arduino.h>

#include "config_store.h"
#include "led_manager.h"
#include "network_manager.h"
#include "plant_controller.h"
#include "serial_console.h"
#include "time_service.h"

namespace {

ConfigStore gConfigStore;
TimeService gTimeService;
PlantController gPlantController(gConfigStore, gTimeService);
NetworkManager gNetworkManager(gConfigStore, gPlantController, gTimeService);
LedManager gLedManager;
SerialConsole gSerialConsole(gPlantController, gNetworkManager);

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  gConfigStore.begin();
  gTimeService.begin();
  gLedManager.begin();
  gPlantController.begin();
  gNetworkManager.begin();
  gSerialConsole.begin();
}

void loop() {
  const unsigned long now = millis();

  gSerialConsole.tick();
  gNetworkManager.tick(now);
  gPlantController.tick(now);

  const PlantStatusSnapshot plantStatus = gPlantController.snapshot(now);
  const NetworkStatusSnapshot networkStatus = gNetworkManager.snapshot();
  gLedManager.tick(now, networkStatus.state, plantStatus.pumpRunning,
                   gNetworkManager.shouldShowError());
}
