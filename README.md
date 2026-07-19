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
> **Each device folder is a separate firmware project.** The five sensor and
> controller projects use Arduino CLI, while
> [`thermostat-mcp9600-simulator`](thermostat-mcp9600-simulator/) is a separate
> ESP-IDF project used to test the thermostat without a physical MCP9600
> temperature sensor. Start with the two detailed guides linked below when
> working on the thermostat or its simulator.

| Folder | What it does |
|---|---|
| [`dht-light/`](dht-light/) | ESP32 sensor firmware that reads a DHT22 temperature/humidity sensor and a Grove digital light sensor, then publishes their values over MQTT. |
| [`airquality-pir/`](airquality-pir/) | ESP32 sensor firmware for Grove air-quality readings and PIR motion detection. |
| [`barometer/`](barometer/) | ESP32 sensor firmware that reads atmospheric pressure from a XENSIV pressure sensor. |
| [`ac-beko/`](ac-beko/) | ESP32 controller firmware that receives signed MQTT commands and controls a Beko air conditioner through COOLIX infrared commands. |
| [`ac-lg/`](ac-lg/) | ESP32 controller firmware that receives signed MQTT commands and controls an LG air conditioner through LG infrared commands. |
| **[`thermostat/`](thermostat/)** | **Offline-first ESP32 thermostat firmware.** It reads an MCP9600 thermocouple, applies heating or cooling hysteresis, safely drives HEAT/COLD/FAN/PUMP outputs, persists its configuration, and connects to the platform through MQTT. **Read the [complete thermostat guide](thermostat/README.md) before changing its control or output-safety logic.** |
| **[`thermostat-mcp9600-simulator/`](thermostat-mcp9600-simulator/)** | **Standalone ESP-IDF firmware for a second ESP32-S3 that emulates an MCP9600 over I2C.** Use it to supply adjustable temperatures to the thermostat during bench testing without the real sensor. **See the [simulator setup, wiring, build, and flashing guide](thermostat-mcp9600-simulator/README.md).** |

Every Arduino firmware folder also contains a `tests/` directory with its
host-side CMake/GoogleTest suite. Generated `build/` directories contain local
build artifacts and are not source projects.


## :open_book: Documentation :open_book:

Take a look here [home-anthill/docs](https://github.com/home-anthill/docs)


## :fire: Releases :fire:

GitHub releases [HERE](https://github.com/home-anthill/sensors/releases)

Versions:

- ??/06/2026 - 5.0.0
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
