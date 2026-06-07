# Changelog

## 5.0.0

### Features

- Added device feature specs to registration payloads so UI clients can render booleans, numeric ranges, and list options from firmware metadata.
- Added optional SSD1306 OLED display support, controlled by `OLED_DISPLAY`.
- Added in-memory feature value tracking for display rendering.
- Added `online` feature registration and periodic online heartbeat publishing.

### Bug fixes

- Replaced duplicated `mode` and `fanSpeed` numeric literals with `IRremoteESP8266` protocol constants in `ac-beko` and `ac-lg` registration specs and command dispatch.
- Updated command dispatch to call `IRremoteESP8266` setters with the protocol constants directly.

### Tests

- Added host tests for `feature_values` and OLED display modules.
- Expanded main `.ino` tests for feature specs, online status publishing, feature value recording, and command value display updates.

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
