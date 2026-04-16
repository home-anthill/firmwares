# Changelog — Code Quality, Bug Fixes & Security Improvements

All changes applied uniformly across all 8 firmware directories (`dht-light`, `barometer`, `airquality-pir`, `power-outage`, `ac-beko`, `ac-lg`, `thermostat`) unless noted.

---

## Memory Safety

**Use-after-free in `build_register_payload` — all `registration.cpp`**
`build_register_payload` returned a pointer to a stack-local `char result[2048]`, leaving the caller with a dangling pointer. Fixed by changing the signature to accept a caller-owned buffer with an explicit size: `void build_register_payload(char* register_payload, size_t max_len, ...)`.

**Unsafe `strcpy` for MAC address — all `wifi_handler.cpp`**
`strcpy(mac_address, mac)` had no bounds check and used `.c_str()` on a temporary `String`. Fixed with a named variable and `strncpy` with explicit null termination.

**Unsafe `strcpy` in `get_feature_uuid_by_name` — all `.ino` files**
`strcpy(featureUuid, uuidval)` could overflow, and `uuidval`/`nameval` were not null-checked. Fixed with `strncpy` + null termination and null guards on both pointers.

**Unchecked `malloc` return — all `registration.cpp`**
`get_register_url()` result was used without a null check; OOM would cause a null dereference. Fixed with an explicit `nullptr` guard in the caller, returning early with an error code.

**`serializeJson` without size limit — all `mqtt_handler.cpp`**
`serializeJson(payloadMsg, payload_to_send)` wrote to a fixed 562-byte buffer without bounds enforcement. Fixed by passing `sizeof(payload_to_send)` as the third argument.

---

## Null Pointer Safety

**Uninitialized buffer in `get_feature_uuid_by_name` — all `.ino` files**
On no-match, `featureUuid` was left uninitialized and passed to `mqtt_notify_value`. Fixed by changing the return type to `bool`, null-terminating the buffer on failure, and guarding all call sites with `if (get_feature_uuid_by_name(...))`.

**Null dereference on JSON response fields — all `registration.cpp`**
Fields `uuid`, `mac`, `manufacturer`, `model` from the HTTP registration response were passed to `strcmp()` without null checks. Fixed with an explicit null check that returns error code `3` on any missing field.

**Null dereference in IR command parsing — `ac-beko/ir_beko.cpp`, `ac-lg/ir_lg.cpp`**
`featureName` from MQTT JSON was used in `strcmp()` without null validation; a malformed payload crashed the device. Fixed with a null guard and `continue`.

**Null dereference in thermostat controller — `thermostat/controller.cpp`**
In `get_setpoint()`, `get_tolerance()`, and `set_configuration()`, JSON-extracted strings `f_feature_name`, `f_model`, `f_api_token` were passed to `strcmp()` without null checks. Fixed with null guards before every `strcmp` call.

**Inbound command authorization not bound to device identity — `thermostat/controller.cpp`, `ac-lg/ir_lg.cpp`, `ac-beko/ir_beko.cpp`**
Inbound MQTT command payloads were validated only against shared `apiToken` and `model`, but not against the specific target device. `deviceUuid`, `mac`, and `featureUuid` were present in the payloads yet not enforced, so any actor able to publish a valid command for the product line could spoof commands to another registered device of the same model.

Fixed by binding command execution to the registered device identity and feature set:
- `thermostat`: `set_configuration()` now requires `deviceUuid == saved_device_uuid`, `mac == device MAC`, and a `featureUuid`/`featureName` pair that matches the thermostat's registered features.
- `ac-lg` / `ac-beko`: `ir_send_command()` now enforces the same checks before applying any IR state change.

The `.ino` MQTT callbacks for those controller firmwares were updated to pass the saved UUID, current MAC, and registered features into the command handlers. Added host-side regression tests covering rejection of wrong `deviceUuid`, wrong `mac`, wrong `featureUuid`, and mismatched `featureUuid`/`featureName` combinations.

---

## Crash & Recovery

**Stack overflow from unbounded recursion — all `registration.cpp`**
`register_server()` called itself recursively on `http_response_code <= 0` with only a `delay(60000)` before each retry, exhausting the stack on persistent failure. Converted to an iterative `do/while` with a max of 10 retries, calling `ESP.restart()` after exhaustion.

**Infinite hang on sensor not found — `thermostat/temp_sensor.cpp`**
`while (1);` on MCP9600 not-found caused a permanent hang with no recovery. Changed to `delay(10000); ESP.restart();`, consistent with the WiFi/MQTT retry-then-restart pattern.

**MQTT stale-connection hang after broker restart — all `mqtt_handler.cpp`**
After a broker restart, the ESP32's TCP socket stayed established while `mqtt_client.connected()` kept returning `true` (socket-layer liveness, not application-layer). The reconnect loop was never entered and the device published nothing indefinitely. Secondary issue: `mqtt_client.connect()` against a port that was open but TLS-not-ready could block unboundedly.

Fixed with two changes:
- `mqtt_init()`: `mqtt_client.setSocketTimeout(15)` to bound TLS hangs.
- `mqtt_notify_value()`: call `mqtt_client.disconnect()` on publish failure so `connected()` returns `false` on the next `loop()` and reconnect is re-entered.

**Faster stale-MQTT detection — all `.ino` files**
`mqtt_client.loop()` return value was discarded. When it returns `false`, PubSubClient has already closed the socket internally (e.g. PINGREQ timeout), but without an explicit `disconnect()` call `connected()` could stay `true` for up to 30–60 s.

Fixed by checking the return in every `loop()` and calling `mqtt_client.disconnect()` on `false`. Sensor firmwares: unconditional check (secondary to publish-failure detection). Thermostat: wrapped in a `connected()` guard to avoid calling `loop()` when offline. Controllers (`ac-beko`, `ac-lg`): this is the *only* stale-detection path since they never publish.

Additionally, `Alarm.delay(1000)` → `Alarm.delay(100)` so MQTT is polled 10× per second instead of once, making keep-alive PINGREQs and stale-connection detection faster without affecting alarm fire times (which are wall-clock based).

---

## Thermostat: Offline-First Redesign — `thermostat/`

Three compounding problems:

1. **Blocking reconnects froze the control loop.** `wifi_connect()` and `mqtt_connect()` used bare `delay()` inside retry loops. Because `Alarm.delay()` (driving the 10 s temperature alarm) was only called at the end of `loop()`, any reconnect attempt froze HEAT/COLD/FAN/PUMP outputs for the entire duration.
2. **Infinite reboot loop.** `wifi_connect()` called `ESP.restart()` after 300 s and was called from `setup()`. If the AP was unreachable, the thermostat never finished `setup()` and rebooted every ~5 minutes indefinitely.
3. **No offline fallback.** There was no mechanism to stop connection attempts and let the thermostat operate offline.

**Fix: offline-first design with a non-blocking connectivity state machine.**

`setup()` now enables alarms *before* any WiFi call. WiFi is started non-blocking (`WiFi.begin()` only). Registration and MQTT happen lazily via `handle_connectivity()` in `loop()`, which advances one step per iteration so `Alarm.delay()` is always reached.

| State | Action | Limit |
|---|---|---|
| `CONN_WIFI_WAITING` | Poll `WiFi.status()` each tick | 30 polls ≈ 30 s |
| `CONN_REGISTERING` | One HTTP POST attempt every 10 s | 3 attempts |
| `CONN_MQTT_TRYING` | One `mqtt_try_connect_once()` every 5 s | 10 attempts |
| `CONN_ONLINE` | Health check only | — |
| `CONN_COOLDOWN` | Do nothing, then `ESP.restart()` | 43 200 s (12 h) |

Exhausting any phase's budget transitions to `CONN_COOLDOWN`, capping reboots to ~2/day.

**Boot-time WiFi false positive fix — `thermostat/thermostat.ino`**
The first version of the thermostat state machine still consumed WiFi "polls" on every `loop()` iteration instead of on a real retry interval. Because the thermostat runs `Alarm.delay(100)` in `loop()`, the `CONN_WIFI_WAITING` budget was being spent about 10 times per second, so a nominal 30-poll window could be exhausted in roughly 3 seconds during boot.

Fixed by time-gating WiFi polling with `conn_next_attempt_ms`, so attempts are only counted after the retry window elapses. The WiFi boot grace period was also rebalanced to poll every 2 seconds for up to 45 polls, giving about 90 seconds before entering the long offline cooldown. Added thermostat host-side tests covering both sides of the bug: rapid loop iterations no longer burn attempts, and cooldown is entered only after the full timed WiFi budget is spent.

New thermostat-specific helpers:
- `wifi_start_connect()` — non-blocking `WiFi.begin()`; `wifi_populate_mac()` — reads MAC once up.
- `mqtt_try_connect_once()` — single attempt, no internal retry or delay.
- `register_secure_server_once()` / `register_insecure_server_once()` — single HTTP attempt, no retry, no delay, no `ESP.restart()`. Share a private `register_once_impl()` helper; the original blocking `register_secure_server()` / `register_insecure_server()` are unchanged for other firmwares.

---

## JSON / Preferences Robustness

**Missing `deserializeJson` error checking — all `storage.cpp`**
`deserializeJson()` return value was ignored; corrupted or empty Preferences JSON caused silent invalid-data access. Fixed with an empty-string early return and a `DeserializationError` check that logs the error and returns. Applied to `storage_get_features()` (all 7 firmwares) and `storage_get_feature_values()` (thermostat only).

---

## Idiomatic Arduino & Code Quality

**Deprecated `StaticJsonDocument` / `DynamicJsonDocument` — multiple files**
ArduinoJson v7 unifies both into `JsonDocument`. Migrated all usages in `registration.cpp`, `mqtt_handler.cpp`, `ir_beko.cpp`, `ir_lg.cpp`, and `thermostat/controller.cpp`.

**Variable name typo `featureNamelval` — `ac-beko/ir_beko.cpp`, `ac-lg/ir_lg.cpp`**
Extra `l` in variable name. Renamed to `featureNameval` throughout both files.

**Wrong log message in `mqtt_subscribe` — all `mqtt_handler.cpp`**
Subscribe function logged `"mqtt_notify_value - publishing topic=%s"` (copy-paste error). Corrected to `"mqtt_subscribe - subscribing topic=%s"`.

**Misleading WiFi retry comment — all `wifi_handler.cpp`**
Comment claimed "100 retries" but the guard was `wifi_retries > 300`. Corrected to "300 retries".

**Format string bug — `dht-light/dht-light.ino`**
`"%.2f %\n"` contained a lone `%`, which is an invalid format specifier. Escaped as `"%.2f %%\n"`.

---

## Tests

**GoogleTest-based host unit tests — all 7 firmware directories**

Each firmware directory now contains a `tests/` subdirectory with a CMake build that runs on the host (no ESP32 hardware or Arduino SDK required). GoogleTest v1.14.0 and ArduinoJson v7.4.2 are fetched automatically via CMake `FetchContent`. Mock headers under `tests/mocks/` stub out the entire ESP32/Arduino SDK surface (`Arduino.h`, `WiFi.h`, `WiFiClientSecure.h`, `PubSubClient.h`, `Preferences.h`, `TimeAlarms.h`, and all sensor/peripheral headers) so production `.cpp` files compile and link on the host unchanged.

Each firmware compiles separate test executables registered with CTest:

| Executable | What is tested |
|---|---|
| `test_storage` | `storage_set_uuid` / `storage_get_uuid`, `storage_set_features` / `storage_get_features`, independence of the two keys, overwrite and empty-array semantics |
| `test_registration` | HTTP registration happy path (200 / 409), JSON field null checks, `malloc` null guard, `DeserializationError` handling, return-code contract |
| `test_mqtt_handler` | `mqtt_notify_value` topic routing (sensor vs online), payload structure, negative values, disconnect-on-publish-failure, no-disconnect on success; `mqtt_connect` happy path |
| `test_main_ino` | `get_feature_uuid_by_name` (match, miss, empty array, null fields, buffer truncation), `buildFeatures()` shape and field values, sensor alarm callbacks (`read_*_sensor_value`) including NaN/not-found guard paths |
| `test_dht_sensor` | DHT22 temperature and humidity reads via mock sensor |
| `test_light_sensor` | TSL2561 lux reads via mock sensor (`dht-light` only) |
| `test_barometer_sensor` | Pressure reads via mock XENSIV sensor (`barometer` only) |
| `test_airquality_sensor` | TVOC/eCO2 reads via mock sensor (`airquality-pir` only) |
| `test_pir_sensor` | PIR state reads via mock GPIO (`airquality-pir` only) |
| `test_ir_beko` | IR command dispatch, null `featureName` guard, `featureNameval` rename (`ac-beko` only) |
| `test_ir_lg` | Same as above for LG protocol (`ac-lg` only) |
| `test_controller` | Setpoint/tolerance get/set, hysteretic control logic (HEAT/COLD/FAN/PUMP outputs), `set_configuration` token/model validation, null-field guards (`thermostat` only) |
| `test_temp_sensor` | MCP9600 thermocouple reads via mock I2C (`thermostat` only) |
| `test_display` | OLED display update logic via mock SSD1306 (`thermostat` only) |

`test_main_ino` uses a thin `main_ino_wrapper.cpp` to compile the `.ino` file as C++; all module dependencies are replaced by stubs defined inside the test file itself, giving fine-grained control over sensor return values, MQTT capture, and storage state. `setup()` and `loop()` compile and link but are never called from tests (full I/O orchestration is not unit-testable without hardware).

Build and run:
```bash
cd <firmware>/tests
cmake -B build && cmake --build build
cd build && ctest --output-on-failure
```
