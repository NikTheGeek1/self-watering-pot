# Smart Pot Hardware Test Runbook

Environment variables
- `SMART_POT_HOST`
- `SMART_POT_USER`
- `SMART_POT_PASSWORD`
- `SMART_POT_SERIAL_PORT`
- `SMART_POT_OTA_PASSWORD`

Scripted checks
- `python3 scripts/test_hardware/smoke.py status`
- `python3 scripts/test_hardware/smoke.py status-schema`
- `python3 scripts/test_hardware/smoke.py auth-check`
- `python3 scripts/test_hardware/smoke.py manual-water`

Manual checklist
- Confirm a fresh device with no Wi-Fi credentials starts the setup AP.
- Confirm invalid credentials fall back to setup AP after 3 attempts.
- Confirm `smart-pot.local` resolves after successful provisioning.
- Confirm the pump physically runs for one pulse when manual watering is triggered.
- Confirm green LED patterns for setup, connecting, connected, and OTA.
- Confirm red LED behavior for pump-active and error states.
- Confirm OTA still succeeds on the real device and reconnects after reboot.
- Confirm the power and tubing setup is safe before pump tests.
