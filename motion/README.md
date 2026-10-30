# motion — HLK-LD2410C radar

Polls the radar OUT pin every second, publishes state changes, and sends a
motion alarm on transitions to detection. UART configures the radar at startup.

## Hardware and connections

Use an ESP32 and an HLK-LD2410C.

| Connection | Destination |
|---|---|
| Radar OUT | GPIO5, `MOTION_PIN` |
| Radar TX | GPIO6, actual ESP32 UART RX |
| Radar RX | GPIO7, actual ESP32 UART TX |

The UART macro names are reversed in the current implementation:
`MOTION_UART_TX` is passed as RX, and `MOTION_UART_RX` as TX.
UART runs at 256000 baud, 8N1.

### Configuration

This project includes its own `config.h`; edit it directly. No config template is required.

Edit `motion/config.h` to match your wiring. Set `OLED_DISPLAY=false` if no
display is fitted.

## Supported features

Values match `buildFeatures()`. Steps are registration/UI increments, not
measurement accuracy.

| Feature | Range / allowed values | Step |
|---|---|---|
| `motion` | 0 No detection; 1 Detection | N/A (boolean) |

Sensor reference: [Hi-Link LD2410C specifications and manual downloads](https://hlktech.com/Goods-239.html).

## Limitations

Configuration emphasizes movement: moving gate 8, stationary gate 0, stationary
thresholds 100, and a 2-second no-target timeout. It does not guarantee stationary
occupancy detection or publish distance/person counts. Nominal range is up to
6 m; reflections, fans, and mounting affect detection.

There is no validated fixed warmup/readiness period. After configuration,
walk through the area, stand still, and leave to verify the actual response.
The 2-second timeout is not a warmup time. The current code does not guarantee
a 30-second alarm suppression window. Network reconnects can pause polling.

## Libraries and build

Run these commands from the repository root with Arduino CLI installed:

```sh
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1
arduino-cli lib install "ld2410"@0.2.2
```

For the optional 128×32 SSD1306 OLED (`OLED_DISPLAY=true`), also install:

```sh
arduino-cli lib install "Adafruit GFX Library"@1.12.6 \
  "Adafruit SSD1306"@2.5.17 "Adafruit BusIO"@1.17.4
```

Prepare credentials if missing:

```sh
cp secrets-template motion/secrets.h
# Edit motion/secrets.h: WiFi, MODEL, admission and MQTT settings.
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./motion/motion.ino
```

If changing TLS, keep `SSL`, `SERVER_PORT`, and `MQTT_PORT` consistent.
Replace `<PORT>` with the board's serial port; serial output uses 115200 baud:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port "<PORT>" ./motion/motion.ino
```
## Tests

Requires CMake, a native C++ compiler, and network access on first configuration
to fetch GoogleTest and ArduinoJson. No ESP32 board is required.

```sh
cmake -S motion/tests -B motion/tests/build
cmake --build motion/tests/build --parallel
ctest --test-dir motion/tests/build --output-on-failure
```
