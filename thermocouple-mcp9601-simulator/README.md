# thermocouple-mcp9601-simulator — MCP9601 I2C emulator

This standalone ESP-IDF firmware turns an ESP32-S3 into an I2C peripheral at
address `0x67`. It implements the MCP9601 registers used by the unchanged
Adafruit MCP9601 library in `../thermocouple-mcp9601`.

The project uses the ESP-IDF version-2 I2C slave driver. The register selector
is captured by `on_receive` during the write phase, and `on_request` wakes a
high-priority response task while the ESP32-S3 stretches SCL. This avoids the
stale response produced by Arduino `Wire` during Adafruit BusIO repeated-start
reads.
No Arduino libraries or network credentials are required.

## Requirements

- ESP32-S3 simulator board
- ESP-IDF v5.5.4
- Sensor firmware and simulator connected to separate USB ports
- Shared I2C bus and ground

## Hardware and connections

Use two ESP32-S3 boards, five optional momentary buttons, and an optional
128×32 SSD1306 OLED. Each board uses its own USB connection.

| Connection | Simulator GPIO | Destination |
|---|---|---|
| MCP9601 slave SDA | 39 | Sensor firmware SDA (default GPIO39) |
| MCP9601 slave SCL | 40 | Sensor firmware SCL (default GPIO40) |
| +0.1 °C button | 4 | Button to GND |
| +0.5 °C button | 5 | Button to GND |
| −0.1 °C button | 6 | Button to GND |
| −0.5 °C button | 7 | Button to GND |
| Cycle temperature status | 10 | Button to GND |
| Optional OLED SDA / SCL | 8 / 9 | SSD1306 at `0x3C` |

Share ground and use 3.3 V bus pull-ups. Do not join the boards' USB 5 V rails.
The OLED uses a separate master bus: do not connect it to the GPIO39/40 slave
bus. An absent OLED does not stop emulation.

Each press of a temperature button applies its step after 30 ms debounce.
The dedicated status button on GPIO10 cycles `temperatureStatus`
0→1→2→3→4→5→0, without changing the selected temperature.
All five buttons act once per press; holding a button does not repeat or select
another function. Release before pressing again. Set `BUTTON_STATUS_PIN` in
`config.h` to change the status button GPIO. All buttons connect to GND and use
internal pull-ups.

The OLED header shows `temperature N`, where N is the selected status code;
the large value is the selected normal temperature, retained while faults are
simulated.
Reset restores 25 °C and status 0.

| Code | Scenario | Emulation |
|---|---|---|
| 0 | OK | Normal selected temperature; no probe fault bits |
| 1 | Initialization error | Wrong device ID; reboot the sensor board while selected |
| 2 | Open circuit | Status bit `0x10` |
| 3 | Short circuit | Status bit `0x20` |
| 4 | Open and short | Both fault bits |
| 5 | Invalid reading | Hot-junction register reports 1373 °C, outside consumer range |

Code 1 is a startup test: changing device ID after successful initialization
cannot trigger the consumer's initialization error. After testing code 1,
select another code and reboot the sensor board to reinitialize it. Codes
0 and 2–5 are detected on the next five-second temperature poll after successful
initialization. These are firmware status scenarios, not chip operating modes.

There is no SEND button. A temperature change is available to the sensor firmware immediately and is read on its next polling cycle. The simulator starts at 25.0 C, reports a fixed 25.0 C cold-junction value, and reports zero
for the raw ADC register.

### Configuration

This project includes its own `config.h`; edit it directly. No config template is required.

Edit `thermocouple-mcp9601-simulator/config.h` to match your wiring.

OLED support is enabled by default. If no display is connected, startup logs one warning and the
MCP9601 simulator continues normally.
To exclude the OLED
code completely:

```bash
idf.py menuconfig
```

Open **MCP9601 simulator**, disable **Enable optional SSD1306 OLED display**,
then save and rebuild.

## Limitations

Buttons use debounce; values are available on the next sensor firmware poll. There is no physical warmup. Requested 0.1 °C changes
are rounded to the register format. The output is clamped to the consuming firmware's type-K range, −200 to 1372 °C. The consumer samples every five seconds, so it will not observe every manual step.

Emulation does not reproduce conversion delays, filtering, thermal lag, noise,
physical probe-failure detection or alert-pin behavior. Status includes `0x40` (temperature update) plus the selected probe fault bits. Normal device ID is `0x41`, revision `0x00`; code 1 overrides the ID. Configuration registers are stored for driver readback, but do not change conversion behavior. It validates transport/control scenarios,
not sensor accuracy or real thermal safety. Reset restores the initial value.

## Build and flash on macOS

### Install ESP-IDF on macOS

Install prerequisites:

```bash
brew install cmake ninja dfu-util ccache python git wget flex bison gperf
```

Install the tested ESP-IDF release:

```bash
mkdir -p ~/esp
cd ~/esp
git clone --branch v5.5.4 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

Load the ESP-IDF environment in every new terminal:

```bash
source ~/.espressif/python_env/idf5.5_py3.14_env/bin/activate
source ~/esp/esp-idf/export.sh
```

### Configure and build

From this simulator directory:

```bash
source ~/.espressif/python_env/idf5.5_py3.14_env/bin/activate
source ~/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

`sdkconfig.defaults` enables the version-2 I2C slave driver required by this
project. After `set-target`, confirm the generated configuration contains:

```bash
grep CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2 sdkconfig
```

Expected:

```text
CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2=y
```

If an old `sdkconfig` predates this project, remove only that generated file and run `idf.py set-target esp32s3` again.

### Find the simulator UART

Before connecting the simulator:

```bash
ls /dev/cu.*
```

Connect it and run the command again. The new entry is its UART, commonly one
of:

```text
/dev/cu.SLAB_USBtoUART
/dev/cu.usbserial-10
/dev/cu.usbmodem1101
```

Use the simulator port, not the sensor firmware port.

### Flash through UART

Replace the example port with the simulator's port:

```bash
idf.py -p /dev/cu.SLAB_USBtoUART -b 460800 flash
```

If automatic download mode fails, hold **BOOT**, tap **RESET**, release
**BOOT**, and run the flash command again.

### Read the serial monitor

ESP-IDF monitor decodes logs and resets the board when it starts:

```bash
idf.py -p /dev/cu.SLAB_USBtoUART monitor
```

Exit with `Ctrl+]`.

Build, flash, and monitor can also be combined:

```bash
idf.py -p /dev/cu.SLAB_USBtoUART -b 460800 flash monitor
```

For a plain terminal monitor without IDF decoding:

```bash
screen /dev/cu.SLAB_USBtoUART 115200
```

Exit `screen` with `Ctrl+A`, then `Ctrl+\` and confirm.

### Expected simulator output

```text
I (...) mcp9601_sim: MCP9601 simulator ready at I2C address 0x67
I (...) mcp9601_sim: I2C SDA=39, SCL=40
I (...) mcp9601_sim: RGB LED disabled on GPIO 48
I (...) mcp9601_sim: Buttons connect GPIO 4, 5, 6, 7, or 10 directly to GND
I (...) mcp9601_sim: Current temperature: requested 25.0 C, MCP9601 value 25.0000 C
```

With the OLED connected, startup also reports:

```text
I (...) sim_display: SSD1306 OLED ready at 0x3c on SDA=8, SCL=9
```

Without it, the non-fatal message is:

```text
W (...) sim_display: Optional OLED not found at 0x3c on SDA=8, SCL=9; continuing without display
```

The sensor firmware should initialize normally, publish approximately 25 °C,
and report `temperatureStatus` 0.

## Bench verification and register contract

This is a separate ESP-IDF project, excluded from the Arduino build matrix.
It has no Arduino library dependencies and no CMake/CTest host suite.
With `thermocouple-mcp9601` connected, verify initialization succeeds at `0x67`,
25 °C is reported, and `temperatureStatus` is 0. Check all four temperature
buttons, including holding each: each press must change temperature exactly
once without changing status. Press the GPIO10 status button six times and
verify 0→1→2→3→4→5→0 while the selected temperature stays unchanged. Holding
the status button must advance only once. Check all six codes against the table
above, including the consumer reboot required for code 1.
Return to 0 and verify recovery telemetry.
Check negative temperatures, both bounds, restart, and absent-OLED operation.
Build with OLED enabled and disabled using menuconfig.

There are no registration features or MQTT messages in the simulator itself.
Hot/cold/delta registers use signed 0.0625 °C units; ambient is fixed at 25 °C.
Requested 0.1 °C steps are quantized to that format, which is not an accuracy
claim. No physical probe, conditioning, calibration, response time, or installed
thermal settling is modeled; values are available immediately after an update.

Register reference: Microchip [MCP9600/MCP9601 datasheet DS20005426](https://ww1.microchip.com/downloads/en/DeviceDoc/20005426.pdf),
sections on register descriptions, device identification and status.
The compatibility target is the [Adafruit MCP960x Arduino driver](https://github.com/adafruit/Adafruit_MCP9600),
including `Adafruit_MCP9601` identity checking. This implements the subset used
by the local consumer, not a complete electrical or behavioral chip model.
