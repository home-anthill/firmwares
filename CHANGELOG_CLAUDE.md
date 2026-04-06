# Changelog — Code Quality, Bug Fixes & Security Improvements

All changes applied uniformly across all 7 firmware directories (`dht-light`, `barometer`, `airquality-pir`, `power-outage`, `ac-beko`, `ac-lg`, `thermostat`) unless noted.

---

## Critical: Bugs & Security

### 1. Use-after-free in `build_register_payload` — all `registration.cpp`

`build_register_payload` returned a pointer to a stack-local `char result[2048]` via `*register_payload = result`. After the function returned, the caller held a dangling pointer to freed stack memory.

**Fix:** Changed to accept a caller-owned buffer with explicit size:
```cpp
// Before
void build_register_payload(char** register_payload, ...);
// After
void build_register_payload(char* register_payload, size_t max_len, ...);
```
Caller passes its own `char register_payload[2048]` and `sizeof(register_payload)`. Uses `serializeJson(doc, register_payload, max_len)`.

### 2. Uninitialized buffer in `get_feature_uuid_by_name` — all 7 `.ino` files

On no-match, `featureUuid` was left uninitialized; the caller then passed it to `mqtt_notify_value`.

**Fix:** Changed return type to `bool`. On not-found, null-terminates buffer and returns `false`. All call sites guarded:
```cpp
// Before
get_feature_uuid_by_name(uuid, name);
mqtt_notify_value(..., uuid, ...);
// After
if (get_feature_uuid_by_name(uuid, sizeof(uuid), name)) {
  mqtt_notify_value(..., uuid, ...);
}
```

### 3. Null pointer dereference on JSON response fields — all `registration.cpp`

Fields `uuid`, `mac`, `manufacturer`, `model` from the HTTP registration response were passed to `strcmp()` without null checks; a missing field caused a crash.

**Fix:** Added explicit null check before comparison:
```cpp
if (uuid_value == nullptr || mac_value == nullptr || ...) {
  Serial.println("register_server - error missing fields in response JSON");
  return 3;
}
```

### 4. Null pointer dereference in IR command parsing — `ac-beko/ir_beko.cpp`, `ac-lg/ir_lg.cpp`

`featureName` from MQTT JSON was used in `strcmp()` without null validation; a malformed payload crashed the device.

**Fix:** Added null guard with `continue`:
```cpp
if (featureNameval == nullptr) {
  Serial.println("ir_send_command - skipping entry with null featureName");
  continue;
}
```

### 5. Null pointer dereference in thermostat controller — `thermostat/controller.cpp`

In `get_setpoint()`, `get_tolerance()`, and `set_configuration()`, JSON-extracted strings `f_feature_name`, `f_model`, `f_api_token` were passed to `strcmp()` without null checks.

**Fix:** Added null guards before all `strcmp` calls:
- `get_setpoint()` / `get_tolerance()`: `if (f_feature_name == nullptr) continue;`
- `set_configuration()`: check `f_model`, `f_api_token`, and `f_feature_name` before validation.

### 6. Stack overflow from unbounded recursion — all `registration.cpp`

`register_server()` called itself recursively on `http_response_code <= 0` with only `delay(60000)` before each retry, exhausting the stack on persistent failure.

**Fix:** Converted to iterative `do/while` with max 10 retries; calls `ESP.restart()` after exhaustion (consistent with WiFi/MQTT retry patterns):
```cpp
do {
  // ... HTTP POST ...
  if (http_response_code <= 0) {
    register_retries++;
    if (register_retries >= max_register_retries) ESP.restart();
    delay(60000);
    continue;
  }
  break;
} while (true);
```

---

## High: Safety & Robustness

### 7. Unsafe `strcpy` for MAC address — all `wifi_handler.cpp`

`strcpy(mac_address, mac)` had no bounds check; also used `.c_str()` on a temporary `String` that could be destroyed before use.

**Fix:** Store in a named variable, use `strncpy` with explicit null termination:
```cpp
String mac_str = WiFi.macAddress();
strncpy(mac_address, mac_str.c_str(), 18 - 1);
mac_address[18 - 1] = '\0';
```

### 8. Unsafe `strcpy` in `get_feature_uuid_by_name` — all 7 `.ino` files

`strcpy(featureUuid, uuidval)` could overflow; `uuidval` and `nameval` from JSON were not null-checked before use.

**Fix:** Replaced with `strncpy` + null termination; added null checks:
```cpp
if (uuidval == nullptr || nameval == nullptr) continue;
strncpy(featureUuid, uuidval, max_len - 1);
featureUuid[max_len - 1] = '\0';
```

### 9. Missing `deserializeJson` error checking — all `storage.cpp`

`deserializeJson(doc, val)` return value was ignored; corrupted or empty Preferences JSON caused silent invalid-data access.

**Fix:** Added empty-string early return and `DeserializationError` check:
```cpp
if (val.length() == 0) { preferences.end(); return; }
DeserializationError err = deserializeJson(doc, val);
if (err) {
  Serial.printf("... deserializeJson failed: %s\n", err.c_str());
  preferences.end();
  return;
}
```
Applied to `storage_get_features()` (all 7 firmwares) and `storage_get_feature_values()` (thermostat only).

### 10. Unchecked `malloc` return — all `registration.cpp`

`get_register_url()` result was used without null check; OOM would cause a null dereference.

**Fix:** Added `nullptr` check in the caller:
```cpp
get_register_url(&register_url);
if (register_url == nullptr) {
  Serial.println("register_server - error: failed to allocate register_url");
  return 1;
}
```

### 11. `serializeJson` without size limit — all `mqtt_handler.cpp`

`serializeJson(payloadMsg, payload_to_send)` wrote to a fixed 562-byte buffer without bounds enforcement, risking overflow.

**Fix:** Added `sizeof(payload_to_send)` as third argument:
```cpp
serializeJson(payloadMsg, payload_to_send, sizeof(payload_to_send));
```

### 12. Infinite hang on sensor failure — `thermostat/temp_sensor.cpp`

`while (1);` on MCP9600 not-found caused a permanent hang with no recovery.

**Fix:** Changed to `delay(10000); ESP.restart();` (consistent with WiFi/MQTT retry-then-restart pattern):
```cpp
Serial.println("Sensor not found. Check wiring! Restarting in 10 seconds...");
delay(10000);
ESP.restart();
```

---

## Medium: Code Quality & Idiomatics

### 13. Deprecated `StaticJsonDocument` / `DynamicJsonDocument` — multiple files

ArduinoJson v7 (7.4.2) unifies both into `JsonDocument`. Old templated types compile as deprecated aliases.

**Files migrated:**
- all `registration.cpp`: `StaticJsonDocument<2048>` → `JsonDocument`
- all `mqtt_handler.cpp`: `DynamicJsonDocument(50)` / `DynamicJsonDocument(512)` → `JsonDocument`
- `ac-beko/ir_beko.cpp`: `StaticJsonDocument<250>` → `JsonDocument`
- `ac-lg/ir_lg.cpp`: `StaticJsonDocument<250>` → `JsonDocument`
- `thermostat/controller.cpp`: `StaticJsonDocument<1024>` → `JsonDocument`

### 14. Variable name typo `featureNamelval` — `ac-beko/ir_beko.cpp`, `ac-lg/ir_lg.cpp`

Extra `l` in variable name (`featureNamelval`). **Fix:** Renamed to `featureNameval` throughout both files.

### 15. Wrong log message in `mqtt_subscribe` — all `mqtt_handler.cpp`

Subscribe function logged `"mqtt_notify_value - publishing topic=%s"` (copy-paste from publish function).
**Fix:** Changed to `"mqtt_subscribe - subscribing topic=%s"`.

### 16. Misleading WiFi retry comment — all `wifi_handler.cpp`

Comment said `"after 100 retries (100 * 1 = 300 seconds)"` but code checked `if (wifi_retries > 300)`.
**Fix:** Updated to `"after 300 retries (300 * 1 = 300 seconds)"`.

### 17. Format string bug — `dht-light/dht-light.ino`

`"%.2f %\n"` — lone `%` is an invalid format specifier.
**Fix:** Escaped as `"%.2f %%\n"` to print a literal percent sign.
