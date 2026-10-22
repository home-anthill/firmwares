# ph — ADS1115 pH meter

Measures differential electrode voltage, applies persisted two-point
calibration, and publishes pH every 30 seconds while MQTT is connected.
The supported electrode model is [DFRobot Gravity: 7/24 Industrial Analog pH Meter Kit V2](https://www.dfrobot.com/product-2069.html).

## Hardware and connections

Use an ESP32, ADS1115, the custom high-impedance pH front end (`PH_BUF`/`VBIAS`),
a pH electrode, calibration buffers, and a calibration button.

| Connection | Destination |
|---|---|
| ADS1115 SDA / SCL | GPIO39 / GPIO40, `I2C_SDA` / `I2C_SCL` |
| ADS1115 ADDR | GND, address `0x48` |
| ADS1115 AIN0 / AIN1 | `PH_BUF` / `VBIAS`, through the existing filter |
| Calibration button | GPIO17 to GND, `PH_CALIBRATION_BUTTON_PIN` |

ADC settings are ±1.024 V differential (`GAIN_FOUR`), 16 SPS. Do not connect a
bare electrode directly to the ADC. Reference: [ADS1115 datasheet](https://www.ti.com/lit/ds/symlink/ads1115.pdf).

### Configuration

Edit `ph/config.h` to match your wiring. Set `OLED_DISPLAY=false` if no
display is fitted.

## Supported features

Values match `buildFeatures()`. Steps are registration/UI increments, not
measurement accuracy.

| Feature | Range / allowed values | Step |
|---|---|---|
| `ph` | 0–14 pH | 0.01 pH |

## Calibration

Use the OLED overlay or 115200-baud serial feedback:

1. Hold GPIO17 for 3 seconds to enter selection.
2. Tap to select 4–7 or 7–10; hold 3 seconds to accept.
3. Rinse, immerse in pH 7.00 buffer, wait for stability, and tap.
4. Rinse, use pH 4.01 or 10.01 buffer, wait, and tap.
5. At **Save calibration?**, tap to save or hold 3 seconds to cancel.

Calibration persists in `ph-meter`. Missing/invalid records
suppress pH; there is no default calibration. Failed saves can be retried.

## Limitations

Capture requires 20 samples within 1 mV: about 6 seconds with the normal loop.
This checks electronic stability, not chemical equilibrium. Probe settling
has no fixed time without its model, condition, and solution. Wait for both
stable feedback and an unchanged buffer reading. Use similar calibration/sample
temperatures; there is no automatic temperature compensation.

The ADC conversion takes about 62.5 ms at 16 SPS; the firmware timeout is 200 ms.
It rejects saturation, samples older than 1.5 seconds, invalid/out-of-range pH,
and measurements during calibration. Normal telemetry does not require the
stability flag, so drifting readings can publish. Slopes outside −80 to −35 mV/pH
are rejected during calibration. The 0.01 step is not an accuracy guarantee.

pH indicates acidity/alkalinity. Buffer quality,
probe care affect results. Network reconnects can pause
sampling and calibration-button processing.

## Libraries and build

Run these commands from the repository root with Arduino CLI installed:

```sh
arduino-cli lib install "ArduinoJson"@7.4.3 "PubSubClient"@2.8.0 \
  "TimeAlarms"@1.5.0 "Time"@1.6.1
arduino-cli lib install "Adafruit ADS1X15"@2.6.2
```

In this firmware the 128×32 SSD1306 OLED (`OLED_DISPLAY=true`) display is required, so install:

```sh
arduino-cli lib install "Adafruit GFX Library"@1.12.6 \
  "Adafruit SSD1306"@2.5.17 "Adafruit BusIO"@1.17.4
```

Prepare credentials if missing:

```sh
if [ ! -f ph/secrets.h ]; then
  cp secrets-template ph/secrets.h
fi
# Edit ph/secrets.h: WiFi, MODEL, admission and MQTT settings.
arduino-cli compile --fqbn esp32:esp32:esp32s3 ./ph/ph.ino
```

If changing TLS, keep `SSL`, `SERVER_PORT`, and `MQTT_PORT` consistent.
Replace `<PORT>` with the board's serial port; serial output uses 115200 baud:

```sh
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port "<PORT>" ./ph/ph.ino
```
## Tests

Requires CMake, a native C++ compiler, and network access on first configuration
to fetch GoogleTest and ArduinoJson. No ESP32 board is required.

```sh
cmake -S ph/tests -B ph/tests/build
cmake --build ph/tests/build --parallel
ctest --test-dir ph/tests/build --output-on-failure
```
