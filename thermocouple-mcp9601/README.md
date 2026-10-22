# thermocouple-mcp9601 — thermocouple temperature sensor

Reads a type-K thermocouple through `Adafruit_MCP9601` every 5 seconds and
publishes temperature over MQTT, with an online heartbeat every 60 seconds.
Uses the standard sensor connection flow; sampling pauses during network outages.

## Hardware and connections

Use an ESP32, the MCP9601 thermocouple PCB at `0x67`, and a type-K probe.
The MCP9601 must report device ID `0x41`; MCP9600 is not supported.

| Connection | ESP32-S2/S3 |
|---|---|
| MCP9601 SDA / SCL | GPIO39 / GPIO40 |
| Thermocouple + / − | Board TC+ / TC− terminals |
| Optional SSD1306 SDA / SCL | Same I2C bus |

Edit `thermocouple-mcp9601/config.h` for your wiring and I2C address. OLED and
its button are disabled by default. Use 3.3 V I2C pull-ups; the thermocouple PCB
requires its VSENSE bias network for open/short-circuit detection.

## Supported features

| Feature | Range / allowed values | Step |
|---|---|---|
| `temperature` | −200–1372 °C, type K | 0.0625 °C |

Range and step follow the [Microchip MCP960X datasheet](https://www.microchip.com/content/dam/mchp/documents/OTH/ProductDocuments/DataSheets/MCP960X-Data-Sheet-20005426.pdf).
The step is digital resolution, not accuracy; the probe and its insulation may
have a narrower usable range. Firmware uses 18-bit conversion with filtering off.
It waits 500 ms after enabling conversion before accepting readings. This covers
the nominal 320 ms conversion plus calculation time, not thermal stabilization.
Reliable measurements require the probe and cold-junction terminals to settle;
that time depends on the probe, mounting and temperature change. There is no
universal warm-up time. Keep terminals away from ESP32/regulator heat.

Open/short-circuit status, nonfinite values and values outside the type-K range
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

## Build and run

From the repository root, with Arduino CLI and ESP32 board support installed:

```bash
cp secrets-template thermocouple-mcp9601/secrets.h
cp config-thermocouple-mcp9601-template thermocouple-mcp9601/config.h
# Edit secrets.h and config.h for this device.
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

Host tests cover sensor identity, probe faults, invalid readings, registration,
publishing and shared modules. Verify real probe disconnection/reconnection and
temperature accuracy on hardware.
