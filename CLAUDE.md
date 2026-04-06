# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a monorepo of Arduino/ESP32 firmwares for the **home-anthill** IoT platform. Each subdirectory is an independent firmware for a specific sensor/controller device: `dht-light`, `airquality-pir`, `barometer`, `power-outage`, `ac-beko`, `ac-lg`, `thermostat`.

All firmwares target **ESP32** boards (esp32, esp32s2, esp32s3) and share a common architecture pattern.

## Quick Start

### Setup
```bash
# Install Arduino CLI (macOS)
brew install arduino-cli

# Add ESP32 board support
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.6

# Install all required libraries (run once)
arduino-cli lib install "ArduinoJson"@7.4.2 "PubSubClient"@2.8.0 "TimeAlarms"@1.5.0 "Time"@1.6.1 \
  "Adafruit Unified Sensor"@1.1.15 "DHT sensor library"@1.4.6 "Grove - Digital Light Sensor"@2.0.0 \
  "HttpClient"@2.2.0 "XENSIV Digital Pressure Sensor"@1.0.2 "Grove - Air quality sensor"@1.0.2 \
  "IRremoteESP8266"@2.9.0

# Prepare secrets
cp secrets-template dht-light/secrets.h
# Edit dht-light/secrets.h with real WiFi/MQTT credentials
```

### Common Commands
```bash
# Build a single firmware (e.g., dht-light for esp32)
cd dht-light && arduino-cli compile --fqbn esp32:esp32:esp32 ./dht-light.ino

# Build for different board variants
arduino-cli compile --fqbn esp32:esp32:esp32s2 ./dht-light.ino
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./dht-light.ino

# Upload to connected ESP32 (find port with: arduino-cli board list)
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 ./dht-light.ino

# View serial output (for debugging)
screen /dev/ttyUSB0 115200   # Ctrl+A, Ctrl+\ to exit
# or use Arduino IDE Monitor / PlatformIO Monitor
```

## Build

Firmwares are built using **Arduino CLI** (not PlatformIO). There is no Makefile — compilation is done directly via `arduino-cli compile`.

```bash
# Install ESP32 board support
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.6

# Install required libraries (see .github/workflows/run-build.yml for exact versions)
arduino-cli lib install "ArduinoJson"@7.4.2 "PubSubClient"@2.8.0 "TimeAlarms"@1.5.0 "Time"@1.6.1 \
  "Adafruit Unified Sensor"@1.1.15 "DHT sensor library"@1.4.6 "Grove - Digital Light Sensor"@2.0.0 \
  "HttpClient"@2.2.0 "XENSIV Digital Pressure Sensor"@1.0.2 "Grove - Air quality sensor"@1.0.2 \
  "IRremoteESP8266"@2.9.0

# Build a specific firmware (from its directory)
cd dht-light
arduino-cli compile --fqbn esp32:esp32:esp32 ./dht-light.ino
```

Each firmware directory must contain a `secrets.h` file (gitignored). Copy `secrets-template` to `<firmware>/secrets.h` and fill in real values for development, or use it as-is for CI builds.

## Local Development

### Environment Setup
Each firmware needs a `secrets.h` file. For development:

1. Copy `secrets-template` to the firmware directory:
   ```bash
   cp secrets-template dht-light/secrets.h
   ```
2. Edit the file with your actual WiFi SSID, MQTT broker URL, etc.
3. For CI/testing without real credentials, use `secrets-template` as-is.

The `SSL` define controls whether connections use TLS — when toggling it, also update `SERVER_PORT` (443 or 80) and `MQTT_PORT` (8883 or 1883).

### Debugging
- **Serial output**: Most firmwares print debug logs to Serial at 115200 baud. Use `screen /dev/ttyUSB0 115200` or Arduino IDE's Serial Monitor to view.
- **MQTT debugging**: Verify device registration at `/admission/register` endpoint. Check MQTT topics `sensors/<uuid>/<type>` for published values.
- **WiFi issues**: Enable WiFi debug logging (uncomment `Serial.println()` statements in `wifi_handler.cpp`).
- **Preferences storage**: Use `preferences.h` inspection via Serial to debug persistent storage issues.

### Testing Approach
There are no unit tests — all testing is integration-based on real hardware:
- **Hardware testing**: Deploy to ESP32, connect to local MQTT broker, verify messages appear.
- **Simulated testing** (CI): The GitHub Actions workflow builds all firmwares using `secrets-template` to verify compilation across board variants (esp32, esp32s2, esp32s3).
- **Common test flow**: Start MQTT broker locally (`docker run --name mosquitto -p 1883:1883 eclipse-mosquitto`), flash firmware, subscribe to topics, verify values appear.

### secrets.h fields

```cpp
#define SECRET_SSID "..."       // WiFi SSID
#define SECRET_PASS "..."       // WiFi password
#define MANUFACTURER "..."      // used in registration payload + validated on MQTT command receipt
#define MODEL "..."             // used in registration payload + validated on MQTT command receipt
#define API_TOKEN "..."         // sent in both registration and MQTT publish payloads
#define SSL true                // toggles TLS for HTTP and MQTT; adjust SERVER_PORT / MQTT_PORT accordingly
#define SERVER_DOMAIN "..."     // admission service hostname
#define SERVER_PORT 443        // 443 (SSL) or 80
#define SERVER_PATH "/admission/register"
#define MQTT_URL "..."          // MQTT broker hostname
#define MQTT_PORT 8883          // 8883 (SSL) or 1883
#define MQTT_AUTH true          // enables username/password auth on MQTT connect
#define MQTT_USERNAME "..."
#define MQTT_PASSWORD "..."
```

## Architecture (per firmware)

### Three firmware categories

**Sensor firmwares** (`dht-light`, `barometer`, `airquality-pir`, `power-outage`):
- Use `TimeAlarms` to periodically read sensors and publish to MQTT
- Alarms are created disabled in `setup()`, enabled only when MQTT connects
- `mqtt_callback()` is empty — sensors are read-only
- `loop()` disables alarms on WiFi loss, re-enables on MQTT reconnect

**Controller firmwares** (`ac-beko`, `ac-lg`):
- No `TimeAlarms`; purely event-driven via MQTT messages
- `mqtt_callback()` receives JSON commands and invokes IR remote sending (`ir_send_command`)
- Each uses `IRremoteESP8266` with device-specific IR protocols (COOLIX for Beko, LG protocol for LG)

**Thermostat** — hybrid sensor + controller:
- Reads temperature (MCP9600 thermocouple via I2C) every 10s via alarm, publishes to MQTT
- Receives MQTT commands to set configuration (setpoint, tolerance)
- Drives physical GPIO outputs (HEAT, COLD, FAN, PUMP) using hysteretic control logic
- Has an OLED display module (SSD1306 128x32 via I2C) — `display.cpp/h`
- Extended `storage.cpp` with `storage_get_feature_values()` / `storage_set_feature_values()` for persisting controller state (`"featureValues"` key in Preferences)
- Alarm runs continuously (not disabled on WiFi loss, unlike sensor firmwares)

### Shared modules (identical across all firmwares)

- **`wifi_handler.cpp/h`** — WiFi connection/reconnection. Embeds a CA certificate for TLS. Exposes a global `wifi_client` (either `WiFiClientSecure` or `WiFiClient` based on the `SSL` define).
- **`registration.cpp/h`** — HTTP POST to `/admission/register`. Handles first-boot (HTTP 200) and already-registered (HTTP 409). Stores device UUID and features in Preferences on success.
- **`mqtt_handler.cpp/h`** — MQTT connection and publishing via `PubSubClient`. Publishes to `sensors/<device_uuid>/<type>`, subscribes to `devices/<uuid>/values`.
- **`storage.cpp/h`** — Persistent key-value storage using ESP32 `Preferences` library. Stores device UUID (`"uuid"` key) and features array (`"features"` key) under namespace `"device"`.

### Key flow

1. `setup()`: init WiFi (with optional TLS) → register device via HTTP → init MQTT → create sensor alarms (disabled) or init IR hardware
2. `loop()`: check WiFi/MQTT connectivity (reconnect if needed) → MQTT reconnect enables alarms → alarms periodically read sensors and publish via MQTT
3. Both WiFi and MQTT have retry limits that trigger `ESP.restart()` after prolonged failures

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
{ "apiToken": "...", "deviceUuid": "...", "featureUuid": "...", "payload": { "value": 23.5 } }
```
- Sensor topic: `sensors/<device_uuid>/<type>`
- Online topic: `online/<device_uuid>/features/<feature_uuid>`

**Subscribe** (controller commands): `devices/<uuid>/values`
- Payload for thermostat: JSON array of `[{apiToken, deviceUuid, mac, model, featureUuid, featureName, value}]`
- Validated against `MODEL` and `API_TOKEN` defines before applying

### SSL conditional compilation

The `SSL` define in `secrets.h` controls whether connections use TLS. It's checked via `#if SSL==true` preprocessor directives throughout wifi, registration, and MQTT code. When changing SSL, also update `SERVER_PORT` and `MQTT_PORT`. The `MQTT_AUTH` define independently controls MQTT username/password authentication.

## CI

GitHub Actions workflow (`.github/workflows/run-build.yml`) builds all 7 firmwares across the ESP32 matrix (esp32, esp32s2, esp32s3) using `secrets-template` as a stand-in for `secrets.h`.

## Code Style

- 2 spaces indentation, UTF-8, trailing newline (see `.editorconfig`)
- C/C++ with Arduino framework conventions
- Function names use `snake_case` with module prefix (e.g., `mqtt_connect`, `dht_get_temperature`)
- Global variables are used for shared state (e.g., `saved_device_uuid`, `mqtt_client`, `wifi_client`)
- **ArduinoJson v7**: use `JsonDocument` — `StaticJsonDocument` and `DynamicJsonDocument` are deprecated aliases in v7 and must not be introduced
- **`Alarm.delay()` vs `delay()`**: sensor firmwares must use `Alarm.delay(ms)` in `loop()` instead of bare `delay()` so that TimeAlarms can fire; controller firmwares (`ac-beko`, `ac-lg`) use bare `delay()` since they have no alarms

## Device-Specific Notes

**dht-light**: Reads temperature/humidity (DHT22) and ambient light (Grove digital light sensor). Uses two separate alarms for sensor polling. Common issue: ensure DHT sensor data pin matches `#define` in `dht_sensor.h`.

**barometer**: Reads atmospheric pressure via XENSIV sensor over I2C. Verify I2C address matches your sensor variant.

**airquality-pir**: Dual-sensor device: air quality (TVOC/eCO2) and PIR motion. PIR is read on interrupt, air quality on timer. Ensure PIR pin matches wiring.

**power-outage**: Monitors mains power status (GPIO-based detection). Publishes online heartbeat even when powered down. Critical for home automation resilience.

**ac-beko** / **ac-lg**: IR remote controllers. Each uses device-specific IR protocol. When modifying IR codes, test against real AC unit. IR LED power and frequency are hardware-dependent.

**thermostat**: Most complex firmware. Differences from sensors:
- Uses MCP9600 thermocouple over I2C (higher precision than DHT)
- Drives four GPIO outputs (HEAT, COLD, FAN, PUMP) with hysteretic control
- OLED display (SSD1306) for local feedback
- Persists controller state (`setpoint`, `tolerance`) in `storage.cpp` via `storage_get_feature_values()` / `storage_set_feature_values()` with `"featureValues"` key
- Alarm runs continuously (not disabled on WiFi loss) to maintain heating/cooling safety

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
6. Test compilation: `arduino-cli compile --fqbn esp32:esp32:esp32 ./<device_name>.ino`
