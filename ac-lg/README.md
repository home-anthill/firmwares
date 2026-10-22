# ac-lg — LG infrared controller

Receives signed MQTT controller commands and transmits LG IR frames to a
compatible LG air conditioner using remote model `AKB74955603`. It has no
temperature or environmental sensor.

## Hardware and connections

Use an **ESP32-S3**; ESP32-S2 is not supported for this firmware. Connect an IR transmitter board, or an IR LED with a current-limited
transistor/MOSFET driver.

| Connection | Destination |
|---|---|
| IR driver control | GPIO4, `IR_SEND_PIN` |

### Configuration

Edit `ac-lg/config.h` to match your wiring. Set `OLED_DISPLAY=false` if no
display is fitted.

## Supported features

Values match `buildFeatures()`. Setpoint uses 1 °C increments; power is boolean,
and mode/fan selections use the listed protocol codes.

| Feature | Range / allowed values | Step |
|---|---|---|
| `on` | 0 Off; 1 On | N/A (boolean) |
| `setpoint` | 16–30 °C | 1 °C |
| `mode` | 0 Cool; 1 Dry; 2 Fan; 3 Auto; 4 Heat | N/A (list) |
| `fanSpeed` | 0 Min; 2 Med; 5 Auto; 10 Max | N/A (list) |

Protocol reference: [IRremoteESP8266 2.9.0, `ir_LG.h`](https://github.com/crankyoldgit/IRremoteESP8266/blob/v2.9.0/src/ir_LG.h).

## Limitations

Commands are accepted after setup and MQTT connection. There is no appliance
feedback: displayed settings reflect requested values, which may differ from
the AC after a missed IR frame, manual remote use, or power loss. Verify every
mode and fan setting on your unit; the LG brand alone does not establish
LG compatibility.

## Libraries and build

Run these commands from the repository root with Arduino CLI installed:

```sh
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1
arduino-cli lib install "IRremoteESP8266"@2.9.0
```

For the optional 128×32 SSD1306 OLED (`OLED_DISPLAY=true`), also install:

```sh
arduino-cli lib install "Adafruit GFX Library"@1.12.6 \
  "Adafruit SSD1306"@2.5.17 "Adafruit BusIO"@1.17.4
```

Prepare credentials if missing:

```sh
if [ ! -f ac-lg/secrets.h ]; then
  cp secrets-template ac-lg/secrets.h
fi
# Edit ac-lg/secrets.h: WiFi, MODEL, admission and MQTT settings.
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./ac-lg/ac-lg.ino
```

If changing TLS, keep `SSL`, `SERVER_PORT`, and `MQTT_PORT` consistent.
Replace `<PORT>` with the board's serial port; serial output uses 115200 baud:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port "<PORT>" ./ac-lg/ac-lg.ino
```
## Tests

Requires CMake, a native C++ compiler, and network access on first configuration
to fetch GoogleTest and ArduinoJson. No ESP32 board is required.

```sh
cmake -S ac-lg/tests -B ac-lg/tests/build
cmake --build ac-lg/tests/build --parallel
ctest --test-dir ac-lg/tests/build --output-on-failure
```
