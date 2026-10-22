# water-level — SEN0368 threshold detector

Publishes whether liquid reaches the sensor's mounting point every second.
Transitions to detection send an alarm, including an initial reading already
above threshold. It does not measure continuous height or volume.

## Hardware and connections

Use an ESP32, DFRobot SEN0368, the custom conversion circuit, and a compatible
nonmetallic container.

| Connection | Destination |
|---|---|
| Sensor level output | Custom conversion-circuit input |
| Converted output | GPIO17, `WATER_LEVEL_PIN` |

The converter must provide defined 0/3.3 V logic in both states; GPIO17 uses
`INPUT` without a pull resistor. Do not connect the sensor's 5–12 V output
directly to the ESP32. Set `WATER_LEVEL_ACTIVE_HIGH=false` if the circuit inverts
the signal.

### Configuration

Edit `water-level/config.h` to match your wiring. Set `OLED_DISPLAY=false` if no
display is fitted.

## Supported features

Values match `buildFeatures()`. Steps are registration/UI increments, not
measurement accuracy.

| Feature | Range / allowed values | Step |
|---|---|---|
| `level` | 0 Below threshold; 1 At/above threshold | N/A (boolean) |

Sensor reference: [DFRobot SEN0368 specification and wiring guide](https://wiki.dfrobot.com/sen0368).

## Limitations

Specified response is 500 ms. With 1-second polling, a crossing may take about
1.5 seconds to publish, excluding network/circuit delays; this is an estimate,
not a guaranteed latency. There is no firmware warmup gate or debounce.

Verify empty/full states after mounting and sensitivity adjustment. Metal,
deposits, container walls, and sloshing can affect switching or cause repeated
alarms. There is no validated universal startup settling time. The sensor's
nominal ±1.5 mm switching error is not depth resolution. A single detector
cannot report remaining litres. Network reconnects can pause polling.

## Libraries and build

Run these commands from the repository root with Arduino CLI installed:

```sh
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1
```

For the optional 128×32 SSD1306 OLED (`OLED_DISPLAY=true`), also install:

```sh
arduino-cli lib install "Adafruit GFX Library"@1.12.6 \
  "Adafruit SSD1306"@2.5.17 "Adafruit BusIO"@1.17.4
```

Prepare credentials if missing:

```sh
if [ ! -f water-level/secrets.h ]; then
  cp secrets-template water-level/secrets.h
fi
# Edit water-level/secrets.h: WiFi, MODEL, admission and MQTT settings.
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./water-level/water-level.ino
```

If changing TLS, keep `SSL`, `SERVER_PORT`, and `MQTT_PORT` consistent.
Replace `<PORT>` with the board's serial port; serial output uses 115200 baud:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port "<PORT>" ./water-level/water-level.ino
```
## Tests

Requires CMake, a native C++ compiler, and network access on first configuration
to fetch GoogleTest and ArduinoJson. No ESP32 board is required.

```sh
cmake -S water-level/tests -B water-level/tests/build
cmake --build water-level/tests/build --parallel
ctest --test-dir water-level/tests/build --output-on-failure
```
