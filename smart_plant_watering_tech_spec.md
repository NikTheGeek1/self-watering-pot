# Smart Plant Watering Project — Technical Build Specification

## Purpose
Hardware handoff document for coding and final integration.

## System Summary
A 12V power supply feeds a Gravity MOSFET power controller and a 5V buck converter. The buck converter powers the ESP32 in standalone operation. The ESP32 reads a capacitive soil moisture sensor, drives the MOSFET control input, and controls two status LEDs. In the verified working wiring, the MOSFET module switches the pump positive side through `VOUT`, while the pump negative stays on common ground.

## Bill of Materials
- ESP32 Development Board – DEVKIT V1 / ESP32-DevKitC
- Gravity Analog Soil Moisture Sensor – Corrosion Resistant
- Gravity MOSFET Power Controller
- Peristaltic Liquid Pump 12V DC – Tube 3x5mm, Flow 80ml/min
- Power Supply 12V 4A – Jack 5.5/2.1
- DC-DC Converter Step-Down 5V 1A
- DC Power Jack 5.5 x 2.1mm Barrel
- 1N4007 rectifier diode
- Silicone Tube Transparent 3x5mm
- 5mm Red LED + 200Ω resistor
- 5mm Green LED + 200Ω resistor
- Hookup wire / jumper wires
- 3D printed enclosure / brackets / tube holder / reservoir mount

## Fixed GPIO Assignment
- Red LED: GPIO14
- Green LED: GPIO13
- MOSFET control signal: GPIO26
- Soil moisture analog input: GPIO34

## Non-GPIO Power Pins
- Sensor + -> ESP32 3.3V
- Sensor - -> GND
- MOSFET control-side + -> ESP32 3.3V
- MOSFET control-side GND -> GND
- Buck OUT+ -> ESP32 5V/VIN
- Buck GND -> ESP32 GND

## Power Architecture
Single external power source.
- 12V adapter positive is the shared +12V rail.
- 12V adapter ground is the shared GND rail.
- Pump positive is supplied through the MOSFET module `VOUT` terminal.
- Buck converter reduces 12V to 5V for the ESP32 only.
- All grounds are common.

## Exact Connectivity

### 1) Main 12V rail
**+12V rail connects to:**
- MOSFET VIN+
- Buck IN+

**GND rail connects to:**
- Pump `-`
- MOSFET GND
- Buck GND
- ESP32 GND

Note: the buck board only has `IN+`, `OUT+`, and `GND`. That single `GND` pin is both input negative and output negative.

### 2) Buck converter to ESP32
- Buck `OUT+` -> ESP32 `5V/VIN`
- Buck `GND` -> ESP32 `GND`
- Do **not** connect 12V directly to the ESP32.

USB-assisted bench testing note:
- If the ESP32 is connected to a computer by USB while the 12V system is also powered, temporarily disconnect Buck `OUT+` from ESP32 `5V/VIN`.
- Keep the grounds common during this test setup.

### 3) Pump and MOSFET power path
- MOSFET `VIN+` -> shared `+12V` rail
- MOSFET `VOUT` -> Pump `+`
- Pump `-` -> shared `GND` rail
- MOSFET `GND` -> shared `GND` rail

Switched path:
`+12V rail -> MOSFET VIN/VOUT -> Pump -> GND rail`

Working module note:
- The verified Gravity MOSFET module exposes `VIN`, `GND`, and `VOUT`.
- In this build, `VOUT` is the switched positive-side output to the pump.

### 4) Flyback diode across pump
- 1N4007 diode across pump terminals
- Diode stripe (cathode) -> Pump `+` / MOSFET `VOUT` side
- Diode non-striped side -> Pump `-` / shared `GND` side

### 5) MOSFET control-side pins
- MOSFET `SIG` -> ESP32 `GPIO26`
- MOSFET `+` -> ESP32 `3.3V`
- MOSFET `GND` -> shared `GND`

### 6) Soil moisture sensor pins
Sensor labels are `A`, `+`, `-`
- Sensor `A` -> ESP32 `GPIO34`
- Sensor `+` -> ESP32 `3.3V`
- Sensor `-` -> shared `GND`
- Sensor should run from 3.3V, not 5V

### 7) LEDs
**Red LED**
- ESP32 `GPIO14` -> `200Ω resistor` -> LED long leg `+`
- LED short leg `-` -> shared `GND`

**Green LED**
- ESP32 `GPIO13` -> `200Ω resistor` -> LED long leg `+`
- LED short leg `-` -> shared `GND`

LED grounds may return to any common ground point, including MOSFET ground.

## Recommended Software Contract
- Read raw analog moisture from GPIO34 and expose it for calibration.
- Store dry and wet calibration values and compute a calibrated moisture percentage.
- Drive GPIO26 high/low to control the pump through the MOSFET.
- Use GPIO14 red LED for watering state or error state.
- Use GPIO13 green LED for normal idle / healthy state.
- Start with conservative pump runtime, for example 1 to 3 seconds max per cycle.
- Add a cooldown delay after watering before reading moisture again.
- Treat moisture thresholds as calibration values, not universal constants.
- Keep automatic watering disabled until calibration is complete and the user explicitly enables it.
- Provide serial status output and manual override commands for testing.

## Physical Build Notes
- Use one compact +12V distribution point and one compact GND distribution point.
- Keep pump and tubing physically separated from exposed electronics where practical.
- Keep wires from buck converter to ESP32 short.
- Route tubing as: reservoir -> pump inlet -> pump outlet -> plant.
- Use a 3D printed tube holder or spike at the plant side.
- Water reservoir can be a printed tank, bottle, or jar with feed tube and vent.

## Pre-Power Sanity Checks
- No 12V line goes directly to ESP32 pins.
- Buck OUT+ goes only to ESP32 5V/VIN.
- Pump `+` is on MOSFET `VOUT`, not directly on `+12V`.
- Pump `-` is on shared GND.
- Diode stripe faces the pump `+` / MOSFET `VOUT` side.
- All grounds are connected together.
- LEDs each have a 200Ω resistor in series.
- Sensor power is 3.3V.
