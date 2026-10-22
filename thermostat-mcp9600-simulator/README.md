# thermostat-mcp9600-simulator — MCP9600 I2C emulator

This standalone ESP-IDF firmware turns an ESP32-S3 into an I2C peripheral at
address `0x67`. It implements the MCP9600 registers used by the unchanged
Adafruit MCP9600 library in `../thermostat`.

The project uses the ESP-IDF version-2 I2C slave driver. The register selector
is captured by `on_receive` during the write phase, and `on_request` wakes a
high-priority response task while the ESP32-S3 stretches SCL. This avoids the
stale response produced by Arduino `Wire` during Adafruit BusIO repeated-start
reads.
No Arduino libraries or network credentials are required.

## Requirements

- ESP32-S3 simulator board
- ESP-IDF v5.5.4
- Thermostat and simulator connected to separate USB ports
- Shared I2C bus and ground

## Hardware and connections

Use two ESP32-S3 boards, four optional momentary buttons, and an optional
128×32 SSD1306 OLED. Each board uses its own USB connection.

| Connection | Simulator GPIO | Destination |
|---|---|---|
| MCP9600 slave SDA | 39 | Thermostat SDA (default GPIO39) |
| MCP9600 slave SCL | 40 | Thermostat SCL (default GPIO40) |
| +0.1 °C button | 4 | Button to GND |
| +0.5 °C button | 5 | Button to GND |
| −0.1 °C button | 6 | Button to GND |
| −0.5 °C button | 7 | Button to GND |
| Optional OLED SDA / SCL | 8 / 9 | SSD1306 at `0x3C` |

Share ground and use 3.3 V bus pull-ups. Do not join the boards' USB 5 V rails.
The OLED uses a separate master bus: do not connect it to the GPIO39/40 slave
bus. An absent OLED does not stop emulation.

There is no SEND button. A temperature change is available to the thermostat immediately and is read on its next polling cycle. The simulator starts at 25.0 C, reports a fixed 25.0 C cold-junction value, and reports zero
for the raw ADC register.

### Configuration

Edit `thermostat-mcp9600-simulator/config.h` to match your wiring.

OLED support is enabled by default. If no display is connected, startup logs one warning and the
MCP9600 simulator continues normally.
To exclude the OLED
code completely:

```bash
idf.py menuconfig
```

Open **MCP9600 simulator**, disable **Enable optional SSD1306 OLED display**,
then save and rebuild.

## Limitations

Buttons use debounce; values are available on the next thermostat poll. There is no physical warmup. Requested 0.1 °C changes
are rounded to the register format. Large jumps can be rejected by the
thermostat's history filter; use small steps for ordinary control tests.

Emulation does not reproduce conversion delays, filtering, thermal lag, noise,
probe failures, or alert-pin behavior. It validates transport/control scenarios,
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

Use the simulator port, not the thermostat port.

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
I (...) mcp9600_sim: MCP9600 simulator ready at I2C address 0x67
I (...) mcp9600_sim: I2C SDA=39, SCL=40
I (...) mcp9600_sim: RGB LED disabled on GPIO 48
I (...) mcp9600_sim: Buttons connect GPIO 4, 5, 6, or 7 directly to GND
I (...) mcp9600_sim: Current temperature: requested 25.0 C, MCP9600 value 25.0000 C
```

With the OLED connected, startup also reports:

```text
I (...) sim_display: SSD1306 OLED ready at 0x3c on SDA=8, SCL=9
```

Without it, the non-fatal message is:

```text
W (...) sim_display: Optional OLED not found at 0x3c on SDA=8, SCL=9; continuing without display
```

The thermostat should initialize normally and then report:

```text
Hot Junction: 25.00
Cold Junction: 25.00
ADC: 0 uV
```