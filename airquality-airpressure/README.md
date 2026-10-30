# airquality-airpressure — SCD43 and BME688

Publishes measured SCD43 CO₂ every 5 seconds and BME688 pressure/BSEC estimates
every 30 seconds. Altitude is calculated from pressure using a fixed 1013.25 hPa
sea-level reference. BSEC runs in every loop at about a 3-second cadence.

## Hardware and connections

Use an ESP32, SCD43, and BME688 I2C sensor boards. BSEC owns the BME688 heater;
do not initialize another driver on the same sensor.

| Connection | Destination |
|---|---|
| Both sensors SDA | GPIO39, `I2C_SDA` |
| Both sensors SCL | GPIO40, `I2C_SCL` |
| SCD43 address | `0x62` |
| BME688 address | `0x76`, fallback `0x77` |

### Configuration

This project includes its own `config.h`; edit it directly. No config template is required.

Edit `airquality-airpressure/config.h` to match your wiring. Set `OLED_DISPLAY=false` if no
display is fitted.

## Supported features

Values match `buildFeatures()`. Steps are registration/UI increments, not
measurement accuracy.

| Feature | Range / allowed values | Step |
|---|---|---|
| `co2` | 0–40000 ppm | 1 ppm |
| `airpressure` | 300–1100 hPa | 0.01 hPa |
| `altitude` | −1000–10000 m | 0.5 m |
| `iaq` | 0–500 IAQ | 1 |
| `voc` | 0–1000 ppm, breath VOC equivalent | 0.001 ppm |
| `eco2` | 0–40000 ppm, CO₂ equivalent | 1 ppm |

## Limitations

The [SCD4x datasheet](https://sensirion.com/media/documents/48C4B7FB/67FE0194/CD_DS_SCD4x_Datasheet_D1.pdf)
specifies SCD43 accuracy over 400–5000 ppm. First samples take about 5 seconds;
typical 63% CO₂ response is 60 seconds. Roughly 3 minutes for 95% response is
an idealized estimate, not an enclosure settling guarantee. Default
self-calibration assumes fresh air near 400 ppm for >3 minutes weekly and
measurement runs of at least 4 hours. Zero and >40000 ppm readings are rejected.

The [BME688 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf)
conditions gas specifications on at least five days of ambient-air operation.
Bosch's [BSEC2 example](https://github.com/boschsensortec/Bosch-BSEC2-Library/blob/master/examples/generic_examples/basic/basic.ino)
suggests 24-hour conditioning for new sensors. There is no fixed calibration
countdown: firmware publishes gas estimates at accuracy >0, including low
accuracy. Learning state is not persisted and restarts after reboot. Pressure
does not require gas conditioning; samples older than 10 seconds are rejected.

`voc` and `eco2` are gas-derived equivalents, not individual gas concentrations;
use SCD43 `co2` for measured CO₂. IAQ/gas trends can indicate changing exposure,
not identify its source. Fixed-reference altitude also changes with weather.
Altitude/gas table limits are UI choices, not verified accuracy ranges.
Network reconnects can interrupt BSEC cadence.

## Libraries and build

Run these commands from the repository root with Arduino CLI installed:

```sh
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1
arduino-cli lib install "Sensirion I2C SCD4x"@1.1.0 "bsec2"@1.10.2610
```

For the optional 128×32 SSD1306 OLED (`OLED_DISPLAY=true`), also install:

```sh
arduino-cli lib install "Adafruit GFX Library"@1.12.6 \
  "Adafruit SSD1306"@2.5.17 "Adafruit BusIO"@1.17.4
```

Prepare credentials if missing:

```sh
cp secrets-template airquality-airpressure/secrets.h
# Edit airquality-airpressure/secrets.h: WiFi, MODEL, admission and MQTT settings.
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./airquality-airpressure/airquality-airpressure.ino
```

If changing TLS, keep `SSL`, `SERVER_PORT`, and `MQTT_PORT` consistent.
Replace `<PORT>` with the board's serial port; serial output uses 115200 baud:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port "<PORT>" ./airquality-airpressure/airquality-airpressure.ino
```
## Tests

Requires CMake, a native C++ compiler, and network access on first configuration
to fetch GoogleTest and ArduinoJson. No ESP32 board is required.

```sh
cmake -S airquality-airpressure/tests -B airquality-airpressure/tests/build
cmake --build airquality-airpressure/tests/build --parallel
ctest --test-dir airquality-airpressure/tests/build --output-on-failure
```
