# Smart Plant ESP32 Controller

This firmware runs the Smart Pot controller described in `smart_plant_watering_tech_spec.md`.

## What It Does
- Reads the soil moisture probe on `GPIO34`.
- Stores dry and wet calibration points in `Preferences`.
- Computes a calibrated moisture percentage.
- Runs bounded manual or automatic pump pulses on `GPIO26`.
- Adds Wi-Fi provisioning through a protected setup AP.
- Adds a protected LAN dashboard for status, settings, and calibration.
- Adds Wi-Fi OTA updates with `ArduinoOTA`.
- Publishes the device on the local network as `smart-pot.local`.

## Safe Defaults
- Auto mode is `OFF` at every boot.
- Manual or automatic pump cycles are clamped to `250-3000 ms`.
- The default pump pulse is `1200 ms`.
- The default cooldown after watering is `45000 ms`.
- The default sample interval is `5000 ms`.
- Wi-Fi setup uses a protected SoftAP when credentials are missing or fail 3 times in a row.

## Local Build Setup
PlatformIO was installed into the local virtual environment at `.venv/`.

Build:

```bash
.venv/bin/pio run
```

Flash once the ESP32 is connected:

```bash
.venv/bin/pio run --target upload
```

Open the serial monitor:

```bash
.venv/bin/pio device monitor
```

## Wi-Fi Provisioning
- If no saved Wi-Fi credentials exist, the controller starts a setup AP named `Smart-Pot-Setup-XXXX`.
- Connect to that AP using the shared firmware secret from `src/app_constants.h`.
- Open `http://192.168.4.1` and submit the local Wi-Fi SSID and password.
- If station mode fails 3 times, the controller falls back to the setup AP automatically.

## LAN Dashboard And OTA
- After joining Wi-Fi, the dashboard is available at `http://smart-pot.local/`.
- The LAN UI and API use HTTP Basic Auth:
  - Username: `smartpot`
  - Password: the same shared secret used for setup AP and OTA
- OTA uses `ArduinoOTA` with hostname `smart-pot`.

## Serial Commands
- `help` or `h` or `?` prints help.
- `read` or `m` prints a moisture reading immediately.
- `status` or `s` prints plant and network state.
- `pump` or `p` runs one manual pump pulse.
- `diag` or `d` drives `GPIO26` HIGH for `2000 ms`, then LOW for `2000 ms`.
- `cal dry` saves the current reading as the dry calibration point.
- `cal wet` saves the current reading as the wet calibration point.
- `cal clear` clears the stored calibration and disables auto mode.
- `set threshold <n>` sets the dry threshold percentage, clamped to `5-95`.
- `set pulse <ms>` sets the pump runtime, clamped to `250-3000`.
- `set cooldown <ms>` sets the minimum delay after watering.
- `set sample <ms>` sets the periodic sensor sample interval.
- `auto on` enables automatic watering if calibration is valid.
- `auto off` disables automatic watering.
- `wifi clear` erases only the saved Wi-Fi credentials and reopens setup AP mode.

## Calibration Workflow
1. Place the moisture probe in your dry reference state and send `cal dry`, or use the dashboard.
2. Place the probe in your wet reference state and send `cal wet`, or use the dashboard.
3. Send `status` or open the dashboard to confirm both calibration values are stored.
4. Choose a threshold with `set threshold <n>` or the dashboard form.
5. Enable automatic watering with `auto on` or the dashboard.

## Safety Notes
- During USB-assisted testing, keep the ESP32 on USB power and leave the `buck OUT+ -> ESP32 5V/VIN` wire disconnected to avoid feeding the board from USB and the buck at the same time.
- The firmware never starts automatic watering on boot.
- OTA start places the pump logic into a lock state until reboot, so no new watering actions can begin mid-update.
- Keep the tubing placed safely during manual pump tests.
- Confirm calibration before trusting automatic watering decisions.
