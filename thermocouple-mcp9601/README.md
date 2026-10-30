# thermocouple-mcp9601 — thermocouple temperature sensor

Reads a configurable thermocouple (type K by default) through `Adafruit_MCP9601` every 5 seconds and
publishes temperature over MQTT, with an online heartbeat every 60 seconds.
Uses the standard sensor connection flow; sampling pauses during network outages.

## Hardware and connections

Use an ESP32, an MCP9601 thermocouple PCB at `0x67`, and a matching thermocouple probe (type K by default).
The MCP9601 must report device ID `0x41`; MCP9600 is not supported.

| Connection | ESP32-S3 |
|---|---|
| MCP9601 SDA / SCL | GPIO39 / GPIO40 |
| Thermocouple + / − | Board TC+ / TC− terminals |
| Optional SSD1306 SDA / SCL | Same I2C bus |

This project includes its own `config.h`; edit it directly. No config template is required.

Edit `thermocouple-mcp9601/config.h` for your wiring and I2C address. OLED and
its button are disabled by default. Configure the converter address and probe type before building:

```cpp
#define THERMOCOUPLE_I2C_ADDRESS 0x67
#define THERMOCOUPLE_TYPE 'K' // K, J, T, N, S, E, B or R
```

Changing the address or probe type requires rebuilding and reflashing.
The selected type configures the converter. Existing firmware validation and
registration bounds remain −200…1372 °C; they are based on type K and do not
represent the usable range of every selectable probe type. Unsupported selections fail
compilation. Use 3.3 V I2C pull-ups; MCP9601 probe diagnostics require the PCB's
VSENSE bias network.

## Supported features

| Feature | Range / allowed values | Step |
|---|---|---|
| `temperature` | −200–1372 °C (existing type-K bounds) | 0.0625 °C |


Range and step follow the [Microchip MCP960X datasheet](https://www.microchip.com/content/dam/mchp/documents/OTH/ProductDocuments/DataSheets/MCP960X-Data-Sheet-20005426.pdf).
The step is digital resolution, not accuracy; the probe and its insulation may
have a narrower usable range. Firmware uses 18-bit conversion with filtering off.
It waits 500 ms after enabling conversion before accepting readings. This covers
the nominal 320 ms conversion plus calculation time, not thermal stabilization.
Reliable measurements require the probe and cold-junction terminals to settle;
that time depends on the probe, mounting and temperature change. There is no
universal warm-up time. Keep terminals away from ESP32/regulator heat.

MCP9601 open/short-circuit status, nonfinite values and values outside the existing firmware range
skip temperature publication; the next valid reading resumes it. Online status
indicates connectivity, not sensor health. A short between probe wires can act
as a new junction and may remain undetected. The Adafruit driver does not expose
I2C read errors reliably, so this firmware cannot guarantee detection of every
communication failure or frozen value. Failed initialization requires a restart.

## Arduino libraries

```bash
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1 "Adafruit MCP9600 Library"@2.0.4 \
  "Adafruit BusIO"@1.17.4 "Adafruit GFX Library"@1.12.6 "Adafruit SSD1306"@2.5.17
```

The MCP9600 library also contains the `Adafruit_MCP9601` class.

## Temporary Serial-only temperature status

`temperatureStatus` registration, MQTT telemetry, alarms, and cache updates are
currently commented out. Every temperature sample prints `temperatureStatus: N`
to Serial at 115200 baud (codes below). No status feature registration is needed.
Temperature and online MQTT publishing remain active. Sampling still follows the
normal network-dependent sensor flow.

The following describes the commented-out MQTT status behavior for restoration.

## MQTT error alarms (temporarily disabled)

Sensor faults publish a signed alarm on
`alarms/<deviceUuid>/features/<statusFeatureUuid>/temperature-status`, using
the same envelope and HMAC canonical format as thermostat alarms. The signed
feature name is `temperatureStatus`; `payload.value` contains the error code:

| Code | Meaning |
|---|---|
| 0 | Recovery / OK status (no push notification) |
| 1 | MCP9601 initialization failed (missing device or incorrect identity) |
| 2 | MCP9601 open-circuit probe status |
| 3 | MCP9601 short-circuit probe status |
| 4 | MCP9601 open- and short-circuit status bits both set |
| 5 | Nonfinite temperature, or value outside the existing firmware range |

`temperatureStatus` is a separate registered sensor feature with its own UUID.
Every five-second sample publishes its code, including 0, to
`sensors/<deviceUuid>/temperature-status` for normal MongoDB persistence and API
retrieval. Its cached value also updates the optional OLED. Invalid measurements
still skip the `temperature` publication.

Nonzero fault transitions and nonzero status on MQTT reconnection additionally
publish the signed alarm above with a separate nonce. Repeated identical faults
do not repeat alarms between reconnects. Recovery publishes ordinary status
telemetry with value 0 and does not send an alarm. The GUI and app hide a status
card whose value is 0. Online heartbeats remain independent of probe health.
Initialization failure requires reboot before initialization is attempted again.

Existing devices need registration of the new `temperatureStatus` feature before
using this firmware; its UUID is distinct from the `temperature` feature UUID.
No parallel Redis current-status store or custom status API is required. Only `temperatureStatus` is supported by this change.

These alarms report only the checks implemented by this driver and do not add
checked I2C transfers or guarantee detection of every hardware failure.

## Build and run

From the repository root, with Arduino CLI and ESP32 board support installed:

```bash
cp secrets-template thermocouple-mcp9601/secrets.h
# Edit secrets.h and the existing config.h for this device.
arduino-cli compile --fqbn esp32:esp32:esp32s3 thermocouple-mcp9601
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyUSB0 thermocouple-mcp9601
```

Use the matching board identifier and serial port. Monitor Serial at 115200 baud.

## Tests

```bash
cmake -S thermocouple-mcp9601/tests -B thermocouple-mcp9601/tests/build
cmake --build thermocouple-mcp9601/tests/build
ctest --test-dir thermocouple-mcp9601/tests/build --output-on-failure
```

Host tests cover all supported probe type selections, MCP9601 identity, rejection
of MCP9600, probe faults, invalid readings, registration,
publishing and shared modules. Verify real probe disconnection/reconnection and
temperature accuracy on hardware.

MQTT topic names use lowercase kebab-case (`temperature-status`). Registered feature names and HMAC signing retain `temperatureStatus`. Deploy the firmware, producer, and alarm-receiver changes together.
