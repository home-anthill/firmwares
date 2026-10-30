# AGENTS.md

This file provides guidance to AI agents when working with code in this repository.

## Project Overview

This is a monorepo of ESP32 projects for the **home-anthill** IoT platform. The device firmwares are `lux-temp-hum`, `pm`, `motion`, `airquality-airpressure`, `ph`, `water-level`, `ac-beko`, `ac-lg`, `thermostat`, and `thermocouple-mcp9601`. The repository also contains the separate `thermostat-mcp9600-simulator` test project.

The device firmwares use Arduino CLI, target **ESP32** boards (ESP32-S3 only), and share a common architecture pattern. The MCP9600 simulator is an ESP-IDF project for ESP32-S3 and is not part of the Arduino firmware build matrix.

Outbound telemetry is signed with HMAC-SHA256 over `deviceUuid\nfeatureUuid\nfeatureName\ntimestamp\nnonce\npayloadJson`. Keep this canonical format aligned with `consumer` and `alarm-receiver`; do not remove the feature name from the signed material.

## Repository Layout

| Folder | Purpose |
|---|---|
| `lux-temp-hum/` | SHT45 temperature/humidity and TSL2591 light sensor; see `lux-temp-hum/README.md`. |
| `pm/` | SEN65-SIN-T PM1/PM2.5/PM4/PM10, temperature, humidity, VOC Index, and NOx Index; see `pm/README.md`. |
| `motion/` | Motion and online sensor firmware. |
| `airquality-airpressure/` | SCD43 CO2 plus BME688 pressure/altitude and BSEC2 IAQ/VOC/eCO2 firmware. |
| `water-level/` | Digital water threshold sensor on GPIO17 through a custom conversion circuit; see `water-level/README.md`. |
| `ph/` | ADS1115 differential pH sensor with persistent two-point calibration on GPIO17; see `ph/README.md`. |
| `ac-beko/` | MQTT-to-COOLIX infrared controller firmware for Beko air conditioners. |
| `ac-lg/` | MQTT-to-LG infrared controller firmware for LG air conditioners. |
| `thermocouple-mcp9601/` | Basic type-K temperature and online sensor using Adafruit_MCP9601 at 0x67; no thermostat outputs. See its README. |
| `thermostat/` | Offline-first MCP9600 thermostat firmware with persistent configuration, OLED support, hysteretic heating/cooling control, and safety-critical HEAT/COLD/FAN/PUMP outputs. Read `thermostat/README.md` before modifying its control logic, GPIO behavior, or safety mechanisms. |
| `thermostat-mcp9600-simulator/` | Standalone ESP-IDF firmware that makes a second ESP32-S3 emulate the MCP9600 I2C device for thermostat bench testing. Read `thermostat-mcp9600-simulator/README.md` for wiring, configuration, build, flash, and usage instructions. |
| `.github/` | GitHub Actions build and host-test workflow. |

Each Arduino firmware folder contains its own `tests/` host-test project. Generated `build/` directories are local artifacts, not independent source projects. Do not apply Arduino CLI conventions, shared-module assumptions, or `build-all.sh` behavior to `thermostat-mcp9600-simulator`; follow its internal guide and use ESP-IDF commands instead.

## Quick Start

### Setup
```bash
# Install Arduino CLI (macOS)
brew install arduino-cli

# Add ESP32 board support
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.12

# Install all required libraries (run once)
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 "TimeAlarms"@1.5.0 "Time"@1.6.1 \
  "Adafruit Unified Sensor"@1.1.15 "Sensirion I2C SHT4x"@1.1.2 "Sensirion I2C SCD4x"@1.1.0 "Sensirion I2C SEN65"@1.1.0 "Adafruit ADS1X15"@2.6.2 "Adafruit TSL2591 Library"@1.4.5 \
  "bsec2"@1.10.2610 "ld2410"@0.2.2 \
  "IRremoteESP8266"@2.9.0 "Adafruit GFX Library"@1.12.6 "Adafruit SSD1306"@2.5.17 \
  "Adafruit BusIO"@1.17.4 "Adafruit MCP9600 Library"@2.0.4

# Prepare missing credentials without overwriting existing secrets
cp secrets-template lux-temp-hum/secrets.h
# Review/edit the existing lux-temp-hum/config.h using lux-temp-hum/README.md.
# Edit lux-temp-hum/secrets.h with real WiFi/MQTT credentials
```

### Common Commands
```bash
# Build a single firmware (e.g., lux-temp-hum for esp32)
cd lux-temp-hum && arduino-cli compile --fqbn esp32:esp32:esp32s3 ./lux-temp-hum.ino

# Build all firmwares and run all host tests
./build-all.sh

# Run only host tests, or only Arduino firmware builds
./build-all.sh --test-only
./build-all.sh --build-only

# Upload to connected ESP32 (find port with: arduino-cli board list)
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyUSB0 ./lux-temp-hum.ino

# View serial output (for debugging)
screen /dev/ttyUSB0 115200   # Ctrl+A, Ctrl+\ to exit
# or use Arduino IDE Monitor / PlatformIO Monitor
```

## Build

Firmwares are built using **Arduino CLI** (not PlatformIO). There is no Makefile — compilation is done directly via `arduino-cli compile`. See **Quick Start** above for board setup, library install commands, and the compile command pattern.

For a repo-wide local check, use `./build-all.sh`. It prepares missing `secrets.h` files from `secrets-template`, uses each firmware's existing `config.h`, builds all active firmwares for `esp32s3` only, and runs every host unit test. Use `--build-only`, `--test-only`, and `--clean-tests` to narrow the run.

Each Arduino firmware includes its own project-specific `config.h` and needs a gitignored `secrets.h`. There are no config templates to copy. Copy `secrets-template` only when credentials are missing, and edit `config.h` using the local firmware README. Do not overwrite existing headers or print real secrets.

## Local Development

### Environment Setup
Each firmware needs a `secrets.h` file. For development:

1. Prepare missing credentials and review the existing firmware configuration:
   ```bash
   cp secrets-template lux-temp-hum/secrets.h
   # Review/edit the existing lux-temp-hum/config.h using lux-temp-hum/README.md.
   ```
2. Edit the file with your actual WiFi SSID, MQTT broker URL, etc.
3. For host tests/builds without real credentials, use `secrets-template` placeholders and the local README config example; live networking requires real settings.

All explicit GPIO assignments, including I2C SDA/SCL, are configured in each project's `config.h`. The per-firmware README includes the configuration and actual signal connections. Each firmware supplies its own `config.h`; edit that file directly rather than generating it from a template. The shared `secrets-template` provides credentials and connection settings. GPIO39/40 and the RGB/button defaults are board-specific, not universal ESP32 pins. Preserve thermostat output safety and external hardware bias when assigning pins.

The `SSL` define controls whether connections use TLS — when toggling it, also update `SERVER_PORT` (443 or 80) and `MQTT_PORT` (8883 or 1883).

### Debugging
- **Serial output**: Most firmwares print debug logs to Serial at 115200 baud. Use `screen /dev/ttyUSB0 115200` or Arduino IDE's Serial Monitor to view.
- **MQTT debugging**: Verify device registration at `/admission/register` endpoint. Check MQTT topics `sensors/<uuid>/<type>` for published values.
- **WiFi issues**: Enable WiFi debug logging (uncomment `Serial.println()` statements in `wifi_handler.cpp`).
- **Preferences storage**: Use `preferences.h` inspection via Serial to debug persistent storage issues.

### Testing Approach

**Host unit tests** (GoogleTest, no hardware required): every firmware has a `tests/` directory with a CMake build. GoogleTest v1.14.0 and ArduinoJson v7.4.2 are fetched automatically via `FetchContent`. Mock headers under `tests/mocks/` stub the entire ESP32/Arduino SDK so production `.cpp` files compile on the host unchanged. The `.ino` file is compiled as C++ via `main_ino_wrapper.cpp`.

```bash
# Build and run tests for one firmware
cd <firmware>/tests
cmake -B build && cmake --build build
cd build && ctest --output-on-failure

# Run a single test executable directly (faster during development)
./build/test_storage
./build/test_mqtt_handler

# Run all firmware host tests from the repo root
./build-all.sh --test-only
```

Each firmware provides separate test executables per module (`test_storage`, `test_registration`, `test_mqtt_handler`, `test_main_ino`, plus device-specific ones such as `test_temp_hum_sensor`, `test_ir_beko_controller`, `test_controller`, etc.). All are registered with CTest.

**Hardware / integration testing** (still required for sensor accuracy and IR codes):
- Deploy to ESP32, connect to local MQTT broker, verify messages appear.
- Start broker: `docker run --name mosquitto -p 1883:1883 eclipse-mosquitto`

**CI**: The GitHub Actions workflow builds all firmwares using each firmware's existing `config.h` and credentials from `secrets-template` for ESP32-S3 only to verify compilation, then runs the host unit-test suite for each firmware.

### secrets.h fields

```cpp
#define SECRET_SSID "..."       // WiFi SSID
#define SECRET_PASS "..."       // WiFi password
#define MANUFACTURER "..."      // used in registration payload + validated on MQTT command receipt
#define MODEL "..."             // used in registration payload + validated on MQTT command receipt
#define API_TOKEN "..."         // sent in both registration and MQTT publish payloads
#define SSL true                // toggles TLS for HTTP and MQTT; adjust SERVER_PORT / MQTT_PORT accordingly
#define SERVER_DOMAIN "..."     // admission service hostname
#define SERVER_PORT 443         // 443 (SSL) or 80
#define SERVER_PATH "/admission/register"
#define MQTT_URL "..."          // MQTT broker hostname
#define MQTT_PORT 8883          // 8883 (SSL) or 1883
#define MQTT_AUTH true          // enables username/password auth on MQTT connect
#define MQTT_USERNAME "..."
#define MQTT_PASSWORD "..."
```

Thermostat settings in `config.h`:

```cpp
#define OPERATING_MODE 0        // thermostat only: 0 = cooling, 1 = heating
#define COOLING_SHORT_RISE_CHECK_SECONDS 120 // one check after COLD starts
#define COOLING_WIDE_RISE_CHECK_SECONDS 600  // recurring checks while COLD remains active
```

## Architecture (per firmware)

### Three firmware categories

**Sensor firmwares** (`lux-temp-hum`, `pm`, `airquality-airpressure`, `motion`, `ph`, `water-level`, `thermocouple-mcp9601`):
- Use `TimeAlarms` to periodically read sensors and publish to MQTT
- Alarms are created disabled in `setup()`, enabled only when MQTT connects
- `mqtt_callback()` is empty — sensors are read-only
- `loop()` disables alarms on WiFi loss, re-enables on MQTT reconnect

**Controller firmwares** (`ac-beko`, `ac-lg`):
- Use `TimeAlarms` for online heartbeat and optional OLED display rotation; control changes are still event-driven via MQTT messages
- `mqtt_callback()` receives JSON commands and invokes IR remote sending (`ir_send_command`)
- Each uses `IRremoteESP8266` with device-specific IR protocols (COOLIX for Beko, LG protocol for LG)
- For IR-backed list features such as `mode` and `fanSpeed`, use the protocol constants from `IRremoteESP8266` headers in both `buildFeatures()` registration specs and command dispatch (`kCoolix...`, `kLgAc...`, etc.) instead of duplicating fixed numeric values. The published JSON values must remain the protocol values expected by the IR library.
- Signed command replay protection: after HMAC, device identity, model, and feature validation pass, controllers claim the signed nonce in a 32-entry in-memory nonce cache and reject duplicates within `COMMAND_MAX_SKEW_SECS` before executing IR changes.

**Thermostat** — hybrid sensor + controller:
- Reads temperature (MCP9600 thermocouple converter via I2C) every 5s via alarm, publishes to MQTT
- Receives MQTT commands to set configuration (setpoint, tolerance)
- Thermostat command contract: every configuration command contains the complete controller state, including both setpoint and tolerance. `set_configuration()` replaces the persisted `featureValues` array by design; partial controller-state commands are not supported.
- Signed command replay protection: after HMAC, device identity, model, and feature validation pass, the thermostat claims the signed nonce in a 32-entry in-memory nonce cache and rejects duplicates within `COMMAND_MAX_SKEW_SECS` before changing configuration.
- `OPERATING_MODE` selects the only active operating mode at compile time: `0` = cooling and `1` = heating. Any other value must fail compilation.
- In cooling builds, only COLD, PUMP, and FAN are written by the temperature loop; HEAT remains at its startup OFF level. In heating builds, only HEAT is written by the temperature loop; COLD, PUMP, and FAN remain at their startup OFF levels.
- Thermostat state mapping is fixed: `-1` = cooling fault, `0` = sleep, `1` = cold, and `2` = heat. `OPERATING_MODE` is a separate compile-time operating mode and must not be confused with these runtime states. Keep the registration specification, mode cache, telemetry, and alarms aligned with these values.
- Runtime `mode` is registered, cached, and published on transitions while MQTT is connected. Cooling-fault transitions also publish a `thermostat-mode-error` alarm on the mode feature; offline alarms are skipped, not queued. Keep this distinct from compile-time `OPERATING_MODE`.
- Stateful hysteresis applies to the selected operating mode. It stops at the setpoint and, after its first cycle, restarts only at `setpoint + tolerance` for cooling or `setpoint - tolerance` for heating. Preserve `prev_thermostat_mode` across sleep samples.
- Cooling builds run the enabled temperature-rise checks configured by `COOLING_SHORT_RISE_CHECK_SECONDS` and `COOLING_WIDE_RISE_CHECK_SECONDS`. Each must be explicitly defined: 0 disables that check independently, positive values enable it, and undefined or negative values fail compilation. Both may be 0 to disable all rise checks. COLD can drive hardware other than a Peltier, but the checks cover the case where a reversed or damaged Peltier heats the controlled fluid.
- The short check runs once per continuous COLD cycle against the COLD-start temperature. The wide check starts from the same baseline, repeats while COLD stays active, and replaces its baseline after each successful window. Both reset after a normal stop at the setpoint. Checks occur on the first valid 5-second sample at or after each deadline.
- Flat or decreasing temperature passes enabled rise checks; they do not enforce cooling performance. Any measured increase above the relevant baseline latches mode `-1`, disables COLD and PUMP, and requests FAN shutdown through its normal cooldown. The fault remains latched until reboot.
- The rise checks assume trustworthy measurements and do not detect a frozen sensor because an unchanged value appears flat. The separate accepted-reading timeout detects software-cache reuse but cannot detect a frozen value that the driver keeps accepting.
- Host `test_main_ino` coverage must compile and run both `OPERATING_MODE=0` and `OPERATING_MODE=1` variants, plus cooling variants with either rise check disabled and both disabled.
- **HARDWARE SAFETY REQUIREMENT**: every HEAT, COLD, FAN, and PUMP control input must have an external resistor that holds it at the configured inactive level while the ESP32 pin is high-impedance during reset, bootloader execution, USB flashing, startup, or loss of ESP32 power. Use a pull-up for each active-low output and a pull-down for each active-high output. If any `*_ACTIVE_LOW` setting changes, review the corresponding hardware bias. Firmware initialization cannot replace this protection.
- Output startup safety: `outputs_init()` must remain the first hardware action in `setup()`. It preloads each inactive ESP32 output latch with `gpio_set_level()` before `pinMode()` enables the driver.
- Has an OLED display module (SSD1306 128x32 via I2C) — `display.cpp/h`
- Extended `storage.cpp` with `storage_get_feature_values()` / `storage_set_feature_values()` for persisting controller state (`"featureValues"` key in Preferences)
- **Offline-first design**: alarms are enabled in `setup()` *before* any WiFi attempt; the thermostat controls temperature immediately on boot regardless of network state
- WiFi/MQTT are managed by a non-blocking **state machine** (`handle_connectivity()` in `thermostat.ino`) — one step per `loop()` iteration — so `Alarm.delay()` is always reached and the control loop never stalls
- Connectivity state machine phases and attempt budgets:

  | State | Action | Limit |
  |---|---|---|
  | `CONN_WIFI_WAITING` | Poll `WiFi.status()` every 2 s | 45 polls ≈ 90 s |
  | `CONN_REGISTERING` | One HTTP POST attempt every 10 s | 3 attempts |
  | `CONN_MQTT_TRYING` | One `mqtt_try_connect_once()` every 5 s | 10 attempts |
  | `CONN_ONLINE` | Health check only | — |
  | `CONN_COOLDOWN` | Do nothing, then `ESP.restart()` | 43 200 s (12 h) |

  Exhausting any phase's budget transitions to `CONN_COOLDOWN`, capping reboots to ~2/day.
- Thermostat-specific `wifi_handler.cpp` adds `wifi_start_connect()` (non-blocking `WiFi.begin()`) and `wifi_populate_mac()`; `mqtt_handler.cpp` adds `mqtt_try_connect_once()` (single attempt, no retry loop); `registration.cpp` adds `register_secure_server_once()` / `register_insecure_server_once()` (single HTTP attempt, no delay, no restart) — these call a private `register_once_impl()` helper; the blocking `register_secure_server()` / `register_insecure_server()` variants remain for other firmwares

### Repeated infrastructure modules (inspect local differences)

Modules are copied per project, not linked from one shared source directory. Thermostat connectivity and pH display/calibration deliberately differ; do not overwrite them with bulk copies.

- **`wifi_handler.cpp/h`** — WiFi connection/reconnection. Embeds a CA certificate for TLS. Exposes a global `wifi_client` (either `WiFiClientSecure` or `WiFiClient` based on the `SSL` define).
- **`registration.cpp/h`** — HTTP POST to `/admission/register`. Handles first-boot (HTTP 200) and already-registered (HTTP 409). Stores device UUID and features in Preferences on success.
- **`mqtt_handler.cpp/h`** — MQTT connection and publishing via `PubSubClient`. Publishes to `sensors/<device_uuid>/<type>`, subscribes to `devices/<uuid>/values`. Socket timeout is set to 15 s (`setSocketTimeout`) to bound TLS hangs. On publish failure, `mqtt_client.disconnect()` is called explicitly so that `mqtt_client.connected()` returns `false` on the next `loop()` iteration and the reconnect path is entered (a stale TCP socket would otherwise keep `connected()` returning `true` indefinitely).
- **`storage.cpp/h`** — Persistent key-value storage using ESP32 `Preferences` library. Stores device UUID (`"uuid"` key) and features array (`"features"` key) under namespace `"device"`.
- **`feature_values.cpp/h`** — In-memory cache of current feature values keyed by registered feature name. Used by OLED display code and command/sensor updates.
- **`display.cpp/h`** — Optional SSD1306 128x32 OLED output behind the `OLED_DISPLAY` define. Tests can force it on with `HOME_ANTHILL_TEST_OLED_DISPLAY`.

### Key flow

1. `setup()`: init WiFi (with optional TLS) → register device via HTTP → init MQTT → create sensor alarms (disabled) or init IR hardware
   - **Thermostat exception**: alarms are enabled *before* WiFi; WiFi is kicked off non-blocking; registration/MQTT happen lazily via `handle_connectivity()` in `loop()`
2. `loop()`: check WiFi/MQTT connectivity (reconnect if needed) → MQTT reconnect enables alarms → alarms periodically read sensors and publish via MQTT
3. Both WiFi and MQTT have retry limits that trigger `ESP.restart()` after prolonged failures
   - **Thermostat exception**: instead of unbounded retries, a state machine caps each burst then enters a 12 h cooldown; `ESP.restart()` fires only when cooldown expires (~2x/day)

### Registration protocol

Each `.ino` file defines a `buildFeatures()` function that describes the device's capabilities as a JSON array (fields: `type`, `name`, `enable`, `order`, `unit`). This array is sent with the registration POST:

```json
{ "mac": "...", "manufacturer": "...", "model": "...", "apiToken": "...", "features": [...] }
```

The server responds:
- **200 OK** — first boot: response contains `{uuid, mac, manufacturer, model, features[{uuid, type, name, ...}]}`. UUID and full features array (now with UUIDs) are saved to Preferences.
- **409 Conflict** — already registered: UUID is read from Preferences instead.

`register_server()` return codes: `0` = OK, `1` = bad HTTP status, `2` = Preferences write failed, `3` = JSON parse error, `4` = response/request mismatch.

### MQTT message format

**Publish** (sensor value or online heartbeat):
```json
{ "deviceUuid": "...", "featureUuid": "...", "timestamp": 1760000000, "nonce": "...", "signature": "...", "payload": { "value": 23.5 } }
```
- `API_TOKEN` is the HMAC key, not a plaintext field in the current outbound telemetry envelope. The feature name is derived from the publish path and included in the canonical signed material. A valid system clock is required before signing.
- Sensor topic: `sensors/<device_uuid>/<type>`
- Online topic: `online/<device_uuid>/features/<feature_uuid>`

**Subscribe** (controller commands): `devices/<uuid>/values`
- Payload for thermostat/controller command topics: an array of signed JSON entries carrying `deviceUuid`, `mac`, `model`, `featureUuid`, `featureName`, `timestamp`, `nonce`, `signature`, and `payload: {"value": ...}`. `API_TOKEN` supplies the local HMAC key.
- Validated against HMAC signature using `API_TOKEN`, timestamp skew, `MODEL`, device UUID, MAC, and registered feature identity before applying; accepted nonces are cached to block replayed signed commands

### SSL conditional compilation

The `SSL` define in `secrets.h` controls whether connections use TLS. It's checked via `#if SSL==true` preprocessor directives throughout wifi, registration, and MQTT code. When changing SSL, also update `SERVER_PORT` and `MQTT_PORT`. The `MQTT_AUTH` define independently controls MQTT username/password authentication.

## CI

GitHub Actions workflow (`.github/workflows/run-build.yml`) builds all active firmwares for ESP32-S3 only using `secrets-template` for `secrets.h` and each firmware's existing `config.h`, and runs all host CTest suites.

## Code Style

- 2 spaces indentation, UTF-8, trailing newline (see `.editorconfig`)
- Put conditional bodies on separate lines; never write `if (...) { ... }` as one line.
- Log sensor errors directly in the branch that handles them; do not introduce a `log_error` helper.
- C/C++ with Arduino framework conventions
- Function names use `snake_case` with module prefix (e.g., `mqtt_connect`, `sht45_read`)
- Global variables are used for shared state (e.g., `saved_device_uuid`, `mqtt_client`, `wifi_client`)
- **ArduinoJson v7**: use `JsonDocument` — `StaticJsonDocument` and `DynamicJsonDocument` are deprecated aliases in v7 and must not be introduced
- **`Alarm.delay()` vs `delay()`**: all active firmwares use `TimeAlarms` and must use `Alarm.delay(100)` in `loop()` instead of bare `delay()` so alarms can fire. The 100 ms value gives fast MQTT polling (10× per second) without affecting alarm fire times (which are based on elapsed time, not iteration count).
- **`mqtt_client.loop()` return check**: all Arduino firmwares check the return value of `mqtt_client.loop()` and call `mqtt_client.disconnect()` on `false`. For controller firmwares (`ac-beko`, `ac-lg`) it also protects command reception between heartbeat publications. For sensor firmwares it is secondary defense: publish failure already triggers `disconnect()` at the next alarm tick (depending on the firmware's polling interval), but the `loop()` return catches stale connections via PINGREQ timeout (~15 s) without waiting for the next publish.

## Device-Specific Notes

**lux-temp-hum**: Reads temperature/humidity from a Sensirion SHT45-AD1B-R2 and ambient light from a TSL2591. Uses two alarms for sensor polling. Both sensors share the I2C SDA/SCL pins from `config.h`; the SHT45 address is 0x44. Driver and paired-read tests are in `temp_hum_sensor.cpp` and `tests/test_temp_hum_sensor.cpp`.

**pm**: Uses only `Sensirion I2C SEN65` 1.1.0 for physical measurements at 0x6B, SDA4/SCL5 by default, 100 kHz. Read all eight fields together every second after MQTT connects; skip each nonfinite field and discard all fields on read error. Features: `pm1`, `pm25`, `pm4`, `pm10`, `temperature`, `humidity`, `vocIndex`, `noxIndex`, plus common heartbeat. Registration uses SEN6x v1.0 output ranges (PM 0–6000, temperature −40–80, RH 0–100, indices 1–500), not the narrower PM precision range. Initialization failures require reboot. Finite values are not a calibration/readiness guarantee; keep the README timing distinctions.

**airquality-airpressure**: SCD43 CO2 at 0x62 and BME688 on the shared I2C bus. Bosch `bsec2` 1.10.2610 owns the BME688, including its heater, at 0x76/0x77. `barometer_update()` runs every loop; cached samples older than 10 seconds are rejected. Publishes pressure, derived altitude, `iaq` (0–500), `voc` (BSEC breath VOC equivalent ppm), and `eco2` (estimated ppm, distinct from real `co2`). Gas outputs with accuracy 0 are skipped. Do not run a second BME688 driver alongside BSEC2. See its README.

**motion**: HLK-LD2410C OUT is polled every second and publishes on transitions. UART startup uses 256000 baud. `MOTION_UART_TX` is passed as the actual ESP32 RX argument (GPIO6), and `MOTION_UART_RX` as TX (GPIO7); document the effective wiring, not just the macro names. Configuration emphasizes moving targets (stationary gate 0/thresholds 100). Do not promise stationary occupancy, a fixed warmup, or a reliable 30-second alarm suppression window from the current code.

**ph**: ADS1115 at 0x48 reads differential AIN0–AIN1 (PH_BUF minus VBIAS), GAIN_FOUR, 16 SPS. GPIO17 calibrates with a debounced three-second hold/tap state machine. Preserve the `ph-meter` / `cal` version-1 record, slope validation, 20-sample 1 mV stability checks, explicit save confirmation, and rejection of saturated/stale samples. Calibration UI uses a four-line overlay in its local display module. Network modules retain the ordinary sensor architecture; blocking connection attempts can pause local sampling.

**ac-beko** / **ac-lg**: IR remote controllers without environmental sensing or appliance acknowledgment. GPIO4 drives a suitable external emitter driver. COOLIX supports setpoint 17–30 °C; LG supports 16–30 °C and selects remote AKB74955603. List feature values are protocol constants, not continuous ranges. When modifying IR codes, test against the real AC unit. IR LED power and frequency are hardware-dependent; there is no universal appliance settling time.

**thermostat**: Most complex firmware. Differences from sensors:
- Uses `Adafruit_MCP9600` at 0x67 (device ID 0x40), compatible with the thermostat MCP9600 simulator.
- Drives four GPIO outputs (HEAT, COLD, FAN, PUMP) with hysteretic control
- OLED display (SSD1306) for local feedback
- Persists controller state (`setpoint`, `tolerance`) in `storage.cpp` via `storage_get_feature_values()` / `storage_set_feature_values()` with `"featureValues"` key
- Alarm runs continuously (not disabled on WiFi loss) to maintain local temperature control
- Current configuration defaults: `OPERATING_MODE=0`, FAN cooldown 240 seconds, rise checks 0/0 (disabled), accepted-reading timeout 15 seconds; NVS defaults are setpoint 25 °C and tolerance 5 °C
- Temperature filtering timestamps only accepted finite readings, including unchanged readings. Cached fallbacks do not refresh the timestamp. `TEMP_MAX_ACCEPTED_AGE_SECONDS` defaults to 15 and must be positive. No accepted startup measurement or age reaching the timeout latches a separate `sensor_fault` until reboot: HEAT OFF in heating; COLD/PUMP OFF in cooling, with FAN cooldown serviced independently in `loop()` (zero delay means immediate OFF). No flat-temperature shutdown is applied. The Adafruit driver still does not prove I2C success/new conversions; do not describe this as complete sensor-failure protection. Temperature registration −40–200 / 0.01 still has TODO markers and is not a verified sensor/probe specification

## Adding a New Firmware

To add a new device:

1. Create a new directory with the device name (snake_case).
2. Copy shared modules from any existing firmware:
   - `wifi_handler.cpp/h`, `registration.cpp/h`, `mqtt_handler.cpp/h`, `storage.cpp/h`
3. Create device-specific modules (e.g., `sensor_name.h`).
4. Create the main `<device_name>.ino` file with:
   - `setup()`: initialize WiFi → register → MQTT → sensor-specific init
   - `loop()`: handle WiFi/MQTT reconnects, alarm management
   - `mqtt_callback()`: handle commands (empty for sensors, implements control logic for controllers)
   - `buildFeatures()`: define device capabilities as JSON array
5. Add the device to `.github/workflows/run-build.yml` build matrix.
6. Add the device to the `FIRMWARES` array in `build-all.sh`.
7. Create `tests/` with a `CMakeLists.txt` and mocks — copy the closest existing firmware's `tests/` directory as a starting point, then add device-specific test files.
8. Test compilation: `arduino-cli compile --fqbn esp32:esp32:esp32s3 ./<device_name>.ino`
9. Add a project-specific `config.h` for GPIO assignments and firmware options, and write a firmware-specific `README.md` following the documentation rules below. Do not add a config template.
10. Run the local aggregate check: `./build-all.sh --test-only` and, when Arduino CLI is installed, `./build-all.sh --build-only`.

**water-level**: GPIO17 digital threshold input, configured by `WATER_LEVEL_PIN` and `WATER_LEVEL_ACTIVE_HIGH` in `config.h`. `level` is true at/above the detection point; read every second. No continuous level measurement or I2C sensor.


## Firmware documentation rules

Maintain a `README.md` in each of the nine Arduino firmware folders and in
`thermostat-mcp9600-simulator`. Keep each guide specific to that project;
repository-wide architecture and migration history belong in the root guide.

Each guide must include:

- Firmware purpose and actual sampling/control behavior.
- Required sensors, boards, probes/front ends/drivers, power and logic levels,
  actual signal-to-GPIO connections, I2C addresses, optional peripherals, and
  the relevant `config.h` settings. Preserve existing local settings; do not
  imply a GPIO map applies to every ESP32 variant.
- Exact Arduino library names/versions needed by that firmware, automatic
  dependencies, optional OLED libraries, build/upload commands with an explicit
  working directory, and local CMake/CTest commands. The simulator instead uses
  ESP-IDF v5.5.4, no Arduino libraries, and bench tests; it has no host test suite.
- A feature table matching current `buildFeatures()` names, formats, units,
  min/max and step. Omit the common `online` feature from device feature tables.
  For boolean/list features state allowed values and mark numeric range/step
  not applicable; retain protocol codes for IR lists.
- Primary manufacturer datasheet links and sections for sensor limits and
  timing. If a probe, emitter, or custom PCB is unidentified, say so instead of
  inventing a model, schematic, component values, or datasheet. IR controllers
  and the simulator do not have physical sensor measurements.
- Limitations and useful interpretations: distinguish direct measurement from
  inferred altitude, BSEC equivalents, gas indices, requested settings, or
  simulated values. State what a feature cannot establish.
- Readiness timing: separate first conversion, response time, conditioning,
  calibration status, and installed-system settling. Label estimates such as
  three-time-constant settling as inference, never a guaranteed warmup. If no
  defensible time is specified, state the missing evidence and give a practical
  commissioning check.

Never assume registration `step` equals sensor accuracy, or that an output
range equals an accuracy-specified range or permitted operating environment.
Document existing metadata discrepancies explicitly; do not silently change
firmware contracts while editing documentation. Read the actual driver and
configuration before copying timing or pin claims from another project.

**thermocouple-mcp9601**: Basic sensor firmware, temperature every 5 seconds and online every 60 seconds. Also registers `temperatureStatus` as a separate integer sensor (0–5, step 1, unit `-`) and publishes it every 5 seconds through normal sensor telemetry. Nonzero status changes and nonzero status on MQTT reconnection send signed `temperatureStatus` alarms using the status feature UUID; recovery to 0 is telemetry only. Fixed type K, −200–1372 °C conversion range, 0.0625 °C digital step. Rejects OC/SC status and invalid readings; requires the MCP9601 bias network for probe diagnostics. Adafruit does not expose checked I2C reads. Keep thermostat on MCP9600 and its simulator unchanged.

MQTT topic names use lowercase kebab-case. Map `temperature-status` to registered/signed feature `temperatureStatus` at the MQTT boundary; preserve its spelling in AMQP, MongoDB and HMAC input.

## Credential rotation boundary

Profile API-token rotation updates backend credentials, not the compiled `API_TOKEN` in deployed firmware. Updating `secrets.h` and reflashing all affected devices after regeneration is expected operator behavior, not a missing feature. MongoDB rotation commits before the alarm-api migration; a later migration failure can return an error after credentials have changed.

## Simulator CI

The `simulator-build` job in `.github/workflows/run-build.yml` uses `espressif/esp-idf-ci-action@v1`, ESP-IDF v5.5.4, target `esp32s3`, and project path `thermostat-mcp9600-simulator`. Keep the pinned IDF version aligned with the simulator README. This job checks compilation, not I2C behavior on hardware.
