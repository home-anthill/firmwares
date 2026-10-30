# lux-temp-hum — SHT45 and TSL2591

Publishes SHT45 temperature/humidity every 30 seconds and TSL2591 illuminance
every 45 seconds while MQTT is connected.

## Hardware and connections

Use an ESP32 with an SHT45-AD1B-R2 and TSL2591 sensor board.

| Connection | Destination |
|---|---|
| Both sensors SDA | GPIO39, `I2C_SDA` |
| Both sensors SCL | GPIO40, `I2C_SCL` |
| SHT45 address | `0x44` |
| TSL2591 address | `0x29` |

### Configuration

This project includes its own `config.h`; edit it directly. No config template is required.

Edit `lux-temp-hum/config.h` to match your wiring. Set `OLED_DISPLAY=false` if no
display is fitted.

## Supported features

Values match `buildFeatures()`. Steps are registration/UI increments, not
measurement accuracy.

| Feature | Range / allowed values | Step |
|---|---|---|
| `temperature` | −40–125 °C | 0.01 °C |
| `humidity` | 0–100 %RH | 0.01 %RH |
| `light` | 0–88000 lux | 1 lux |

## Limitations

The [SHT4x datasheet](https://sensirion.com/resource/datasheet/sht4x) lists typical
63% response times of 2 seconds for temperature and 4 seconds for humidity.
Enclosure and PCB heat can increase settling time. The heater is unused;
failed/nonfinite paired reads skip both fields, and RH is clipped to 0–100%.

The [TSL2591 datasheet](https://look.ams-osram.com/m/c901de8e97608f8/original/TSL2591-DS000338.pdf)
describes its integration/gain limits. This firmware uses driver defaults of
medium gain and 100 ms integration (about 120 ms per read), without auto-ranging.
There is no warmup beyond conversion time. Bright light can saturate before
the advertised 88000 lux; saturation is not explicitly rejected. Lux is rounded
up to an integer. Light spectra and covers affect the estimate.

Temperature/RH indicate local conditions; lux compares illumination at the
sensor, not UV exposure or plant PAR.
Network reconnects can pause sampling.

## Libraries and build

Run these commands from the repository root with Arduino CLI installed:

```sh
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1
arduino-cli lib install "Sensirion I2C SHT4x"@1.1.2 \
  "Adafruit TSL2591 Library"@1.4.5 "Adafruit Unified Sensor"@1.1.15
```

For the optional 128×32 SSD1306 OLED (`OLED_DISPLAY=true`), also install:

```sh
arduino-cli lib install "Adafruit GFX Library"@1.12.6 \
  "Adafruit SSD1306"@2.5.17 "Adafruit BusIO"@1.17.4
```

Prepare credentials if missing:

```sh
cp secrets-template lux-temp-hum/secrets.h
# Edit lux-temp-hum/secrets.h: WiFi, MODEL, admission and MQTT settings.
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./lux-temp-hum/lux-temp-hum.ino
```

If changing TLS, keep `SSL`, `SERVER_PORT`, and `MQTT_PORT` consistent.
Replace `<PORT>` with the board's serial port; serial output uses 115200 baud:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port "<PORT>" ./lux-temp-hum/lux-temp-hum.ino
```
## Tests

Requires CMake, a native C++ compiler, and network access on first configuration
to fetch GoogleTest and ArduinoJson. No ESP32 board is required.

```sh
cmake -S lux-temp-hum/tests -B lux-temp-hum/tests/build
cmake --build lux-temp-hum/tests/build --parallel
ctest --test-dir lux-temp-hum/tests/build --output-on-failure
```
