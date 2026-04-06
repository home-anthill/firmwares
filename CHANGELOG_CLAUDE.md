# Changelog — Code Quality, Bug Fixes & Security Improvements

All changes below were applied uniformly across all 7 firmware directories (`dht-light`, `barometer`, `airquality-pir`, `power-outage`, `ac-beko`, `ac-lg`, `thermostat`) unless noted otherwise.

---

## Critical: Bugs & Security

### 1. Use-after-free in `build_register_payload` — all `registration.cpp`

`build_register_payload` declared a stack-local `char result[2048]`, serialized JSON into it, then assigned its address to the caller's output pointer (`*register_payload = result`). After the function returned, the caller used a dangling pointer to freed stack memory.

**Fix:** Changed the function signature to accept a caller-owned buffer with an explicit size limit:
```cpp
// Before
void build_register_payload(char** register_payload, ...);
// After
void build_register_payload(char* register_payload, size_t max_len, ...);
```
The caller now passes its own `char register_payload[2048]` and `sizeof(register_payload)`. Serialization uses `serializeJson(doc, register_payload, max_len)` to enforce bounds.

### 2. Uninitialized buffer in `get_feature_uuid_by_name` — all 7 `.ino` files

When no matching feature was found in the loop, the function returned without writing to `featureUuid`. The caller then passed this uninitialized buffer to `mqtt_notify_value`, causing undefined behavior.

**Fix:** Changed return type from `void` to `bool`. On not-found, the buffer is null-terminated and `false` is returned. All call sites now check the return value before using the buffer:
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

After deserializing the HTTP registration response, fields like `uuid`, `mac`, `manufacturer`, `model` were extracted and immediately passed to `strcmp()` without null checks. If any field was missing, this caused a crash.

**Fix:** Added explicit null check before the comparison:
```cpp
if (uuid_value == nullptr || mac_value == nullptr || ...) {
  Serial.println("register_server - error missing fields in response JSON");
  return 3;
}
```

### 4. Null pointer dereference in IR command parsing — `ac-beko/ir_beko.cpp`, `ac-lg/ir_lg.cpp`

`featureName` extracted from the MQTT JSON payload was used directly in `strcmp()` without null validation. A malformed payload would crash the device.

**Fix:** Added null guard with `continue` to skip malformed entries:
```cpp
if (featureNameval == nullptr) {
  Serial.println("ir_send_command - skipping entry with null featureName");
  continue;
}
```

### 5. Null pointer dereference in thermostat controller — `thermostat/controller.cpp`

In `get_setpoint()`, `get_tolerance()`, and `set_configuration()`, JSON-extracted strings (`f_feature_name`, `f_model`, `f_api_token`) were passed to `strcmp()` without null checks.

**Fix:** Added null guards before all `strcmp` calls:
- `get_setpoint()` / `get_tolerance()`: `if (f_feature_name == nullptr) continue;`
- `set_configuration()`: check `f_model`, `f_api_token`, and `f_feature_name` for null before validation.

### 6. Stack overflow from unbounded recursion — all `registration.cpp`

`register_server()` called itself recursively on HTTP failure (`http_response_code <= 0`) with only a `delay(60000)` before each retry. On persistent network failure, this would exhaust the stack and crash.

**Fix:** Converted to an iterative `do/while` loop with a maximum of 10 retries. After exhausting retries, `ESP.restart()` is called (consistent with existing WiFi/MQTT retry patterns):
```cpp
do {
  // ... HTTP POST ...
  if (http_response_code <= 0) {
    register_retries++;
    if (register_retries >= max_register_retries) {
      ESP.restart();
    }
    delay(60000);
    continue;
  }
  break;
} while (true);
```

---

## High: Safety & Robustness

### 7. Unsafe `strcpy` in WiFi MAC address copy — all `wifi_handler.cpp`

`strcpy(mac_address, mac)` was used without bounds checking. Additionally, `WiFi.macAddress().c_str()` returned a pointer to a temporary `String` that could be destroyed before use.

**Fix:** Store the `String` in a named variable and use `strncpy` with explicit null termination:
```cpp
String mac_str = WiFi.macAddress();
strncpy(mac_address, mac_str.c_str(), 18 - 1);
mac_address[18 - 1] = '\0';
```

### 8. Unsafe `strcpy` in `get_feature_uuid_by_name` — all 7 `.ino` files

`strcpy(featureUuid, uuidval)` could overflow if `uuidval` from JSON was longer than expected. Also, `uuidval` and `nameval` from JSON were not checked for null before use.

**Fix:** Replaced with `strncpy` + explicit null termination, added null checks on JSON field pointers:
```cpp
if (uuidval == nullptr || nameval == nullptr) continue;
strncpy(featureUuid, uuidval, max_len - 1);
featureUuid[max_len - 1] = '\0';
```

### 9. Missing `deserializeJson` error checking — all `storage.cpp`

`deserializeJson(doc, val)` was called without checking the return value. If the stored JSON was corrupted or empty, subsequent code operated on invalid data silently.

**Fix:** Added `DeserializationError` check and empty-string early return:
```cpp
if (val.length() == 0) { preferences.end(); return; }
DeserializationError err = deserializeJson(doc, val);
if (err) {
  Serial.printf("... deserializeJson failed: %s\n", err.c_str());
  preferences.end();
  return;
}
```
Applied to `storage_get_features()` in all 7 firmwares and additionally to `storage_get_feature_values()` in the thermostat firmware.

### 10. Unchecked `malloc` return — all `registration.cpp`

In `get_register_url()`, `malloc()` could return `nullptr` on memory exhaustion. The result was used without null checking, and the caller would dereference it.

**Fix:** Added `nullptr` check in the caller after `get_register_url()` returns:
```cpp
get_register_url(&register_url);
if (register_url == nullptr) {
  Serial.println("register_server - error: failed to allocate register_url");
  return 1;
}
```

### 11. `serializeJson` without size limit — all `mqtt_handler.cpp`

`serializeJson(payloadMsg, payload_to_send)` wrote to a fixed 562-byte `char` array without enforcing the buffer size, risking overflow.

**Fix:** Added `sizeof(payload_to_send)` as the third argument:
```cpp
serializeJson(payloadMsg, payload_to_send, sizeof(payload_to_send));
```

### 12. Infinite hang on sensor failure — `thermostat/temp_sensor.cpp`

If the MCP9600 temperature sensor was not found during initialization, `while (1);` caused the device to hang forever with no recovery.

**Fix:** Changed to `ESP.restart()` after a 10-second delay (consistent with WiFi/MQTT retry-then-restart pattern):
```cpp
Serial.println("Sensor not found. Check wiring! Restarting in 10 seconds...");
delay(10000);
ESP.restart();
```

---

## Medium: Code Quality & Idiomatics

### 13. Deprecated `StaticJsonDocument` / `DynamicJsonDocument` — multiple files

ArduinoJson v7 (7.4.2, used by this project) unifies both into `JsonDocument`. The old templated types compile as deprecated aliases but should be migrated.

**Files changed:**
- all `registration.cpp`: `StaticJsonDocument<2048>` → `JsonDocument`
- all `mqtt_handler.cpp`: `DynamicJsonDocument(50)` / `DynamicJsonDocument(512)` → `JsonDocument`
- `ac-beko/ir_beko.cpp`: `StaticJsonDocument<250>` → `JsonDocument`
- `ac-lg/ir_lg.cpp`: `StaticJsonDocument<250>` → `JsonDocument`
- `thermostat/controller.cpp`: `StaticJsonDocument<1024>` → `JsonDocument`

### 14. Variable name typo `featureNamelval` — `ac-beko/ir_beko.cpp`, `ac-lg/ir_lg.cpp`

The variable was consistently misspelled as `featureNamelval` (extra `l` before `val`).

**Fix:** Renamed to `featureNameval` throughout both files.

### 15. Wrong log message in `mqtt_subscribe` — all `mqtt_handler.cpp`

The subscribe function logged `"mqtt_notify_value - publishing topic=%s"` which was a copy-paste error from the publish function.

**Fix:** Changed to `"mqtt_subscribe - subscribing topic=%s"`.

### 16. Misleading WiFi retry comment — all `wifi_handler.cpp`

Comment said `"after 100 retries (100 * 1 = 300 seconds)"` but the code checked `if (wifi_retries > 300)`, which is 300 retries at 1 second each.

**Fix:** Updated comment to `"after 300 retries (300 * 1 = 300 seconds)"`.

### 17. Format string bug — `dht-light/dht-light.ino`

`Serial.printf("... humidity: %.2f %\n", hum)` — the lone `%` is an invalid format specifier.

**Fix:** Escaped as `%%` to print a literal percent sign: `"... humidity: %.2f %%\n"`.
