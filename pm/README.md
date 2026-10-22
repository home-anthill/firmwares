# pm — Sensirion SEN65-SIN-T

Reads all eight SEN65 measurements together every second and publishes finite
fields while MQTT is connected. Read errors discard the sample; individual
unavailable fields are skipped.

## Hardware and connections

Use an ESP32 and an SEN65-SIN-T with a suitable connector/breakout. Keep the
sensor's air inlet/outlet unobstructed.

| Connection | Destination |
|---|---|
| SEN65 SDA | GPIO39, `I2C_SDA` |
| SEN65 SCL | GPIO40, `I2C_SCL` |
| SEN65 address | `0x6B` |

I2C runs at 100 kHz with a 100 ms timeout. Initialization checks presence,
resets the sensor, waits 1200 ms, and starts continuous measurement.

### Configuration

Edit `pm/config.h` to match your wiring. Set `OLED_DISPLAY=false` if no
display is fitted.

## Supported features

Values match `buildFeatures()`. Steps are registration/UI increments, not
measurement accuracy.

| Feature | Range / allowed values | Step |
|---|---|---|
| `pm1` | 0–6000 µg/m³ | 0.1 µg/m³ |
| `pm25` | 0–6000 µg/m³ | 0.1 µg/m³ |
| `pm4` | 0–6000 µg/m³ | 0.1 µg/m³ |
| `pm10` | 0–6000 µg/m³ | 0.1 µg/m³ |
| `temperature` | −40–80 °C | 0.005 °C |
| `humidity` | 0–100 %RH | 0.01 %RH |
| `vocIndex` | 1–500 | 0.1 |
| `noxIndex` | 1–500 | 0.1 |

Ranges/scaling: [SEN6x datasheet v1.0, §§1.2–1.4 and §4.8.6](https://sensirion.com/media/documents/FAFC548D/6AC50BC5/PS_DS_SEN6x.pdf).

## Limitations

| Signal | Datasheet timing |
|---|---|
| PM | Typical startup 30 seconds |
| Temperature / RH | 63% response <60 seconds / <20 seconds |
| VOC / NOx raw event detection | Startup <60 seconds / <300 seconds |
| VOC Index | <1 hour until listed specifications are met |
| NOx Index | <6 hours until listed specifications are met |

NOx may be unavailable for 11 seconds after reset. Firmware does not enforce
conditioning times: a finite reading can publish before specifications are met.
PM precision is specified only over 0–1000 µg/m³; PM4/PM10 are derived from the
particle distribution. Output ranges are not permitted operating conditions.

PM trends can reveal particle events; VOC/NOx indices indicate relative gas
changes, not ppm/ppb concentrations or a particular source. Airflow, installation
heat, and aerosol properties affect interpretation. Network reconnects can
pause polling.

## Libraries and build

Run these commands from the repository root with Arduino CLI installed:

```sh
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1
arduino-cli lib install "Sensirion I2C SEN65"@1.1.0
```

For the optional 128×32 SSD1306 OLED (`OLED_DISPLAY=true`), also install:

```sh
arduino-cli lib install "Adafruit GFX Library"@1.12.6 \
  "Adafruit SSD1306"@2.5.17 "Adafruit BusIO"@1.17.4
```

Prepare credentials if missing:

```sh
if [ ! -f pm/secrets.h ]; then
  cp secrets-template pm/secrets.h
fi
# Edit pm/secrets.h: WiFi, MODEL, admission and MQTT settings.
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./pm/pm.ino
```

If changing TLS, keep `SSL`, `SERVER_PORT`, and `MQTT_PORT` consistent.
Replace `<PORT>` with the board's serial port; serial output uses 115200 baud:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port "<PORT>" ./pm/pm.ino
```
## Tests

Requires CMake, a native C++ compiler, and network access on first configuration
to fetch GoogleTest and ArduinoJson. No ESP32 board is required.

```sh
cmake -S pm/tests -B pm/tests/build
cmake --build pm/tests/build --parallel
ctest --test-dir pm/tests/build --output-on-failure
```
