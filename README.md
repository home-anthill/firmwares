<h1 align="center">
  <br>
  <img src="https://github.com/home-anthill/docs/blob/master/icons/logo512.png?raw=true" alt="ks89/home-anthill" width="220">
  <br>
home-anthill
  <br>
sensors
</h1>


## :compass: Start here: what each folder contains :compass:

> [!IMPORTANT]
> **Each device folder is a separate firmware project.** The sensor and
> controller projects use Arduino CLI, while
> [`thermostat-mcp9600-simulator`](thermostat-mcp9600-simulator/) is a separate
> ESP-IDF project used to test the thermostat without a physical MCP9600
> temperature sensor. Start with the two detailed guides linked below when
> working on the thermostat or its simulator.

| Folder | What it does |
|---|---|
| [`lux-temp-hum/`](lux-temp-hum/) | ESP32 sensor firmware that reads a Sensirion SHT45-AD1B-R2 temperature/humidity sensor and a TSL2591 light sensor over I2C, then publishes their values over MQTT. |
| [`thermocouple-mcp9601/`](thermocouple-mcp9601/) | MCP9601 type-K temperature, temperatureStatus, and online sensor. See its [guide](thermocouple-mcp9601/README.md). |
| [`pm/`](pm/) | Sensirion SEN65-SIN-T PM1/PM2.5/PM4/PM10, temperature, humidity, VOC Index, NOx Index, and online telemetry. See the [SEN65 guide](pm/README.md). |
| [`motion/`](motion/) | ESP32 firmware for HLK-LD2410C motion detection and online heartbeat only. |
| [`airquality-airpressure/`](airquality-airpressure/) | SCD43 CO₂ plus BME688 pressure, altitude, IAQ, VOC equivalent, and eCO₂ using Bosch BSEC2. |
| [`water-level/`](water-level/) | Digital water threshold detection on GPIO17 through a custom conversion circuit, with boolean state and online telemetry. See the [water-level guide](water-level/README.md). |
| [`ph/`](ph/) | ADS1115 differential pH sensor with persistent two-point buffer calibration using a button on GPIO17. See the [pH wiring and calibration guide](ph/README.md). |
| [`ac-beko/`](ac-beko/) | ESP32 controller firmware that receives signed MQTT commands and controls a Beko air conditioner through COOLIX infrared commands. |
| [`ac-lg/`](ac-lg/) | ESP32 controller firmware that receives signed MQTT commands and controls an LG air conditioner through LG infrared commands. |
| **[`thermostat/`](thermostat/)** | **Offline-first ESP32 thermostat firmware.** It reads an MCP9600 thermocouple, applies heating or cooling hysteresis, safely drives HEAT/COLD/FAN/PUMP outputs, persists its configuration, and connects to the platform through MQTT. **Read the [complete thermostat guide](thermostat/README.md) before changing its control or output-safety logic.** |
| **[`thermostat-mcp9600-simulator/`](thermostat-mcp9600-simulator/)** | **Standalone ESP-IDF firmware for a second ESP32-S3 that emulates an MCP9600 over I2C.** Use it to supply adjustable temperatures to the thermostat during bench testing without the real sensor. **See the [simulator setup, wiring, build, and flashing guide](thermostat-mcp9600-simulator/README.md).** |
| [`thermocouple-mcp9601-simulator/`](thermocouple-mcp9601-simulator/) | Standalone ESP-IDF MCP9601 emulator with OLED, four temperature buttons, and a dedicated status button for thermocouple sensor bench testing. See its [guide](thermocouple-mcp9601-simulator/README.md). |


All device firmwares support **ESP32-S3 only** (`esp32:esp32:esp32s3`). Local builds and Arduino CI use this target. The simulator also targets ESP32-S3, using ESP-IDF.

## GPIO configuration

Each Arduino firmware includes its own project-specific `config.h` for GPIO
assignments and firmware options. Edit this existing file directly; config
templates are no longer used. Credentials, device identity, and server connection
settings belong in a separate, gitignored `secrets.h`.

```bash
# From the repository root; replace <firmware-folder> with the project name.
cp secrets-template <firmware-folder>/secrets.h
```

Edit credentials and identity (including `MODEL`) in `secrets.h`, and pins,
OLED options, and device settings in `config.h` before building and flashing.
The single `secrets-template` is shared by all Arduino firmwares; each
`config.h` contains only that project's hardware settings.


## :open_book: Documentation :open_book:

Take a look here [home-anthill/docs](https://github.com/home-anthill/docs)


## :fire: Releases :fire:

GitHub releases [HERE](https://github.com/home-anthill/sensors/releases)

Versions:

- ??/??/2026 - 5.0.0
- 28/05/2026 - 4.0.0
- 25/12/2025 - 3.0.0
- 02/09/2025 - 2.0.0
- 19/05/2024 - 1.0.0
- 11/01/2023 - 1.0.0-beta.1


## :sparkling_heart: A big thank you to :sparkling_heart:

##### the authors of the main icon of this project:

- <a href="https://www.freepik.com/free-vector/underground-ant-nest-with-red-ants_18582279.htm">Image by brgfx</a> from <a href="https://www.freepik.com/" title="Freepik">Freepik</a>


# :copyright: License :copyright:

The MIT License (MIT)

Copyright (c) 2021-2026 Stefano Cappa (Ks89)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

<br/>
