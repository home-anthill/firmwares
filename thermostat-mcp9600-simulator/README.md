# Thermostat MCP9600 Simulator

This standalone ESP-IDF firmware turns an ESP32-S3 into an I2C peripheral at
address `0x67`. It implements the MCP9600 registers used by the unchanged
Adafruit MCP9600 library in `../thermostat`.

The project uses the ESP-IDF version-2 I2C slave driver. The register selector
is captured by `on_receive` during the write phase, and `on_request` wakes a
high-priority response task while the ESP32-S3 stretches SCL. This avoids the
stale response produced by Arduino `Wire` during Adafruit BusIO repeated-start
reads.

## Requirements

- ESP32-S3 simulator board
- ESP-IDF v5.5.4
- Thermostat and simulator connected to separate USB ports
- Shared I2C bus and ground

## Wiring

| Thermostat ESP32-S3 | Simulator ESP32-S3 | Purpose |
|---|---|---|
| GPIO 39 | GPIO 39 | SDA |
| GPIO 40 | GPIO 40 | SCL |
| GND | GND | Common electrical reference |

Do not connect the boards' USB 5 V rails. SDA and SCL must be pulled up to
3.3 V somewhere on the shared bus. The powered OLED or another breakout may
already provide those pull-ups.

Four optional momentary buttons connect their GPIO directly to GND. Internal
pull-ups keep unpressed buttons high.

| Button GPIO | Action |
|---:|---|
| 4 | Increase by 0.1 C |
| 5 | Increase by 0.5 C |
| 6 | Decrease by 0.1 C |
| 7 | Decrease by 0.5 C |

There is no SEND button. A temperature change is available to the thermostat
immediately and is read on its next five-second polling cycle. The simulator
starts at 25.0 C, reports a fixed 25.0 C cold-junction value, and reports zero
for the raw ADC register.

### Optional OLED display

The simulator supports the same 128x32 SSD1306 display type used by the
thermostat. It uses a second I2C controller so the simulator never becomes a
second master on the thermostat/MCP9600 bus.

The two I2C connections have deliberately different roles:

- On GPIO 39/40, the thermostat is the I2C master and the simulator is an I2C
  slave emulating the MCP9600 at address `0x67`.
- On GPIO 8/9, the simulator is the I2C master and the local OLED is an I2C
  slave at address `0x3C`.

Although an I2C bus can normally contain several slaves, placing the local
OLED on GPIO 39/40 would require the simulator to operate as both a slave and
a second master on the thermostat bus. That would introduce multi-master
arbitration and could interfere with the repeated-start timing required by the
MCP9600 emulation. Keeping the OLED on the ESP32-S3's second I2C controller
isolates display traffic from sensor traffic.

GPIO 8/9 were selected because they do not conflict with the simulator's
buttons on GPIO 4-7, MCP9600 connection on GPIO 39/40, UART console on GPIO
43/44, or RGB LED on GPIO 48. They are not mandatory: another free,
output-capable GPIO pair can be used by changing `OLED_SDA_PIN` and
`OLED_SCL_PIN` in `main/display.c`.

| OLED pin | Simulator ESP32-S3 |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

The expected OLED address is `0x3C`. Do not connect the simulator OLED to GPIO
39/40: those pins must remain the MCP9600 slave connection to the thermostat.
If the display has an exposed RESET pin, tie it to the board reset signal or
3.3 V; the common four-pin modules do not need a separate reset connection.

OLED support is enabled by default. If no display is connected, startup logs
one warning and the MCP9600 simulator continues normally. To exclude the OLED
code completely:

```bash
idf.py menuconfig
```

Open **MCP9600 simulator**, disable **Enable optional SSD1306 OLED display**,
then save and rebuild.

## Install ESP-IDF on macOS

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
source ~/esp/esp-idf/export.sh
```

## Configure and build

From this simulator directory:

```bash
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

If an old `sdkconfig` predates this project, remove only that generated file
and run `idf.py set-target esp32s3` again.

## Find the simulator UART

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

## Flash through UART

Replace the example port with the simulator's port:

```bash
idf.py -p /dev/cu.SLAB_USBtoUART -b 460800 flash
```

If automatic download mode fails, hold **BOOT**, tap **RESET**, release
**BOOT**, and run the flash command again.

## Read the serial monitor

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

## Expected simulator output

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
