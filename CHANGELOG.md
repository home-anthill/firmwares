# Changelog

## 5.0.0

### Features

- Added `water-level` digital threshold firmware with GPIO17, configurable polarity, boolean water/online features, and host-test/build integration. `water-level` firmware publishes signed `level` alarms on transitions to the reached state, using the same MQTT alarm publisher as motion; repeated reached readings do not emit duplicate alarms.
- Remove `airquality-motion`.
- Rename `co2-airpressure` to `airquality-airpressure` and use Bosch BSEC2 for numeric `iaq`, `voc`, and `eco2`, keeping measured `co2`, pressure, and altitude.
- Added `thermocouple-mcp9601` with MCP9601 `temperatureStatus` integer sensor telemetry (0–5) with nonzero fault alarms, and error-only status.
- Add a separate ESP32-S3 MCP9600 simulator CI build pinned to ESP-IDF v5.5.4 using Espressif’s build action.
- Publish `pm` gas-index telemetry on voc-index and nox-index topics while preserving registered names in signatures.
- Split local credentials into `secrets.h` from one shared `secrets-template`, and GPIO assignments and firmware options into each firmware's own `config.h`. Removed config templates; edit the project-specific config file directly.
- Renamed `barometer` to `co2-airpressure` and added SCD43-D-R2 CO2 telemetry alongside BME688 pressure/altitude on the shared I2C bus.
- Updated `barometer` to BME688 with the gas heater disabled, automatic I2C address detection, and a paired pressure/altitude reading that rejects failed conversions.
- Added the `ph` sensor firmware with ADS1115 differential measurement, GPIO17 buffer calibration, confirmed persistent calibration records, OLED calibration screens, signed pH/online telemetry, and host-test/build integration.
- Renamed `dht-light` to `lux-temp-hum`, including the sketch, secrets template, build commands, and CI references. Use Sensirion SHT45-AD1B-R2 I2C paired temperature/humidity measurements.
- Renamed `airquality-motion` into `airquality-motion` using HLK-LD2410C module instead of a basic PIR sensor. Changed `airquality-motion` motion polling from 30 seconds to one second and added signed rising-edge motion alarms on `alarms/{deviceUuid}/features/{motionFeatureUuid}/motion`.
- Added signed thermostat mode-error alarms on `alarms/{deviceUuid}/features/{modeFeatureUuid}/thermostat-mode-error`; errors detected while MQTT is disconnected are not replayed.
- Added optional SSD1306 OLED display support, controlled by `OLED_DISPLAY`.
- Added `online` feature registration and periodic online heartbeat publishing.
- Use lowercase kebab-case for MQTT topic names, including `temperature-status`; keep registered and signed feature identity `temperatureStatus`.
- Added device feature specs to registration payloads so UI clients can render booleans, numeric ranges, and list options from firmware metadata.
- Remove ESP32 and ESP32S2 support


### Bug fixes

- Replaced duplicated `mode` and `fanSpeed` numeric literals with `IRremoteESP8266` protocol constants in `ac-beko` and `ac-lg` registration specs and command dispatch.
- Updated command dispatch to call `IRremoteESP8266` setters with the protocol constants directly.
- Automatically reset generated host-test builds with stale CMake paths after firmware or checkout renames.

### Tests

- Add UART and LD2410 mocks for motion sensor host tests, and pin Nick Reynolds’ `ld2410` dependency to 0.2.2 in CI.
- Added host coverage for alarm topics and payloads, PIR edge behavior and polling, and connected/disconnected thermostat alarm publication.
- Added host tests for `feature_values` and OLED display modules.
- Expanded main `.ino` tests for feature specs, online status publishing, feature value recording, and command value display updates.
- Update all tests for the new firmwares

### Chores

- Added `OLED_DISPLAY` to `secrets-template`.


## 4.0.0

Changes apply across all 7 firmware directories (`dht-light`, `barometer`, `airquality-pir`, `ac-beko`, `ac-lg`, `thermostat`) unless a narrower scope is listed.

### Features

- Added `build-all.sh` to prepare missing `secrets.h` files from `secrets-template`, build all firmwares with Arduino CLI, and run host CTest suites. Supports `--build-only`, `--test-only`, and `--clean-tests`.
- Added thermostat offline-first connectivity:
  - `setup()` enables alarms before WiFi.
  - WiFi, registration, and MQTT are advanced lazily through `handle_connectivity()`.
  - Connection states: `CONN_WIFI_WAITING`, `CONN_REGISTERING`, `CONN_MQTT_TRYING`, `CONN_ONLINE`, `CONN_COOLDOWN`.
  - Failed phases enter a 12-hour cooldown before restart, capping reboots to about 2/day.
- Added thermostat non-blocking helpers:
  - `wifi_start_connect()`
  - `wifi_populate_mac()`
  - `mqtt_try_connect_once()`
  - `register_secure_server_once()`
  - `register_insecure_server_once()`
- Kept the original blocking thermostat registration functions unchanged for other firmwares.

### Bug fixes

- Fixed `build_register_payload()` returning a pointer to a stack buffer by requiring a caller-owned output buffer and size.
- Fixed unbounded recursive registration retries by converting retry handling to an iterative loop with a 10-attempt limit and restart after exhaustion.
- Fixed unchecked `malloc` use in registration URL handling by returning early on allocation failure.
- Fixed missing registration response field validation for `uuid`, `mac`, `manufacturer`, and `model`.
- Fixed corrupted or empty Preferences JSON handling by checking `deserializeJson()` errors in `storage_get_features()` and thermostat `storage_get_feature_values()`.
- Fixed `get_feature_uuid_by_name()` miss handling by returning `bool`, null-terminating the output buffer on failure, and guarding all call sites.
- Fixed malformed MQTT payload crashes in `ac-beko`, `ac-lg`, and `thermostat` by null-checking JSON fields before `strcmp()`.
- Fixed MQTT stale-connection recovery:
  - Set MQTT socket timeout to 15 seconds.
  - Disconnect after publish failure so reconnect can run.
  - Check `mqtt_client.loop()` return values and disconnect on failure.
  - Reduced `Alarm.delay(1000)` to `Alarm.delay(100)` for faster MQTT polling.
- Fixed thermostat control loop freezes by removing blocking reconnect loops from normal operation.
- Fixed thermostat infinite boot reboot loop when WiFi is unavailable.
- Fixed thermostat boot WiFi budget burn by time-gating WiFi polling with `conn_next_attempt_ms`; the 45 polls now cover about 90 seconds.
- Fixed permanent thermostat temperature-sensor hang by replacing `while (1);` with delayed restart.
- Fixed `serializeJson()` calls writing to fixed MQTT buffers without an explicit size limit.
- Fixed unsafe MAC address copying by replacing temporary `String.c_str()` plus `strcpy()` with bounded copy and explicit null termination.
- Fixed unsafe `featureUuid` copying by using bounded copy, null termination, and null guards.
- Fixed DHT humidity log format string from `"%.2f %\n"` to `"%.2f %%\n"`.

### Security fixes

- Bound inbound controller commands to device identity and feature identity:
  - `thermostat`, `ac-lg`, and `ac-beko` now require matching `deviceUuid`, device MAC, and valid `featureUuid`/`featureName`.
  - MQTT callbacks now pass saved UUID, current MAC, and registered features into command handlers.
- Added signed MQTT command replay protection for `thermostat`, `ac-lg`, and `ac-beko`:
  - Commands claim a signed nonce only after HMAC, timestamp, device identity, model, and feature validation.
  - A 32-entry in-memory nonce cache rejects duplicate nonces within `COMMAND_MAX_SKEW_SECS`.
- Bound telemetry signatures to feature name/type:
  - HMAC input is now `deviceUuid\nfeatureUuid\nfeatureName\ntimestamp\nnonce\npayloadJson`.
  - Prevents reusing a captured sensor payload under a different feature topic.

### Idiomatic issues

- Migrated ArduinoJson `StaticJsonDocument` and `DynamicJsonDocument` usages to `JsonDocument`.
- Renamed typo `featureNamelval` to `featureNameval` in `ac-beko/ir_beko.cpp` and `ac-lg/ir_lg.cpp`.
- Corrected `mqtt_subscribe()` log text from the publish message to the subscribe message.
- Corrected WiFi retry comment from "100 retries" to "300 retries".

### Chores

- Added GitHub Actions host unit-test job to configure, build, and run each firmware CMake/CTest suite alongside the ESP32 compile matrix.

### Tests

- Added GoogleTest-based host unit tests for all 7 firmware directories.
- Added CMake/CTest test builds under each firmware's `tests/` directory.
- Added CMake `FetchContent` setup for GoogleTest v1.14.0 and ArduinoJson v7.4.2.
- Added ESP32/Arduino SDK mocks under `tests/mocks/` for:
  - `Arduino.h`
  - `WiFi.h`
  - `WiFiClientSecure.h`
  - `PubSubClient.h`
  - `Preferences.h`
  - `TimeAlarms.h`
  - sensor and peripheral headers
- Added common test coverage for:
  - storage UUID/features behavior
  - registration success and error contracts
  - MQTT payload routing, payload shape, publish failure recovery, and connect behavior
  - `get_feature_uuid_by_name()`, feature building, and sensor alarm callbacks
- Added firmware-specific coverage for:
  - DHT22 temperature/humidity reads
  - TSL2561 lux reads
  - XENSIV pressure reads
  - TVOC/eCO2 reads
  - PIR reads
  - Beko and LG IR command dispatch and validation
  - thermostat setpoint/tolerance, hysteresis, configuration validation, temperature sensor, display updates, and offline connectivity timing
- Added regression tests for rejecting controller commands with wrong `deviceUuid`, wrong MAC, wrong `featureUuid`, and mismatched `featureUuid`/`featureName`.
- Added `main_ino_wrapper.cpp` test wrappers so `.ino` files compile as C++ while hardware orchestration remains stubbed.

Build and run one firmware test suite:

```bash
cd <firmware>/tests
cmake -B build && cmake --build build
cd build && ctest --output-on-failure
```
