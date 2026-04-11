# Smart Plant ESP32 Controller

This repo now contains the first real controller firmware for the hardware described in `smart_plant_watering_tech_spec.md`.

## What It Does
- Blinks the green LED on `GPIO18` as a heartbeat so you can confirm the board is running.
- Reads and reports the raw soil moisture ADC value from `GPIO34` at `115200` baud.
- Supports dry and wet calibration points captured from the live sensor.
- Converts the raw moisture reading into a calibrated moisture percentage.
- Runs bounded manual or automatic pump pulses on `GPIO26`.
- Uses the red LED on `GPIO19` only while the pump is active.
- Enforces a minimum cooldown after every watering cycle.
- Keeps automatic watering disabled on boot until you explicitly enable it.

## Safe Defaults
- Auto mode is `OFF` at every boot.
- Manual or automatic pump cycles are clamped to `250-3000 ms`.
- The default pump pulse is `1200 ms`.
- The default cooldown after watering is `45000 ms`.
- The default sample interval is `5000 ms`.

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

## Serial Commands
- `help` or `h` or `?` prints help.
- `read` or `m` prints a moisture reading immediately.
- `status` or `s` prints calibration, pump, and threshold state.
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

## Calibration Workflow
1. Place the moisture probe in your dry reference state and send `cal dry`.
2. Place the probe in your wet reference state and send `cal wet`.
3. Send `status` to confirm both calibration values are stored.
4. Choose a threshold with `set threshold <n>`.
5. Enable automatic watering with `auto on`.

The controller computes moisture as a calibrated percentage between the stored dry and wet raw values, so it works even if your sensor's raw numbers decrease as the soil gets wetter.

## First Automatic Test
1. Keep the plant outlet and reservoir in a safe place.
2. Flash the firmware.
3. Run `cal dry` and `cal wet`.
4. Send `set threshold 35`.
5. Send `auto on`.
6. Watch the serial output and verify that watering only happens when the calibrated moisture percentage falls below the threshold.

## Safety Notes
- During USB-assisted testing, keep the ESP32 on USB power and leave the `buck OUT+ -> ESP32 5V/VIN` wire disconnected to avoid feeding the board from USB and the buck at the same time.
- The firmware never starts automatic watering on boot.
- Keep the tubing placed safely during manual pump tests.
- Confirm calibration before trusting automatic watering decisions.
