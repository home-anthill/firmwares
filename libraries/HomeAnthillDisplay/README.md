# HomeAnthillDisplay

Shared SSD1306 128×32 display engine for the ten Arduino firmwares. It uses the
`ha` namespace and is independent of sketch configuration headers and credentials.
Feature values come directly from the HomeAnthillCore cache.

## Use

The optional `HomeAnthillDisplayConfig.h` bridge declares `display_config` from
settings already loaded by the sketch. It does not include local files itself.

```cpp
#include "config.h"
#include <HomeAnthillDisplay.h>
#include <HomeAnthillDisplayConfig.h>

void setup() {
  ha::display_init(display_config);
}

void loop() {
  ha::display_update();
  // Retain the firmware's connectivity, sensor, and Alarm.delay() scheduling.
}
```

The library manages one OLED through ordinary `ha::display_*` functions.
Render state, button state, and deadlines are private to `display.cpp`, like
the original per-firmware implementation. There is no display wrapper class
or explicit driver allocation/deletion.

`DisplayConfig` selects the I2C bus, SDA/SCL, address (default `0x3C`), reset pin,
button pin/enabled flag, and timings. Defaults preserve a 50 ms debounce,
10-second restart hold, 30-second auto-off, and 1.5-second message persistence.
The Adafruit driver is a function-local static created by the first enabled
`display_init()` call, not during global initialization. Its I2C bus object
and reset pin stay fixed until reboot; initialization with different values is
rejected before accessing hardware. Calling `display_init()` again with the
same driver wiring resets the screen state and timers. Thermostat output
initialization remains the first hardware action in its setup.

`OLED_DISPLAY=false` makes the bridge set `enabled=false`. No display allocation,
I2C access, button GPIO setup, rendering, or display-button restart occurs.
This is a runtime setting: the library and its build dependencies are still
compiled. This differs from the previous local modules' preprocessor exclusion.

## Presentation policies

Pass a `ha::DisplayPolicy` as the optional second argument of `display_init()`.
It provides feature visibility, labels, decimal precision,
message defaults, and connectivity-notice duration. Zero notice duration keeps
connection errors visible; a positive duration returns to feature data after
the notice expires. Callbacks receive `FeatureValue` from HomeAnthillCore.
Configuration and policies are copied; any strings and callback data they
reference must remain valid while the display uses them.

| Firmware | Policy |
|---|---|
| lux-temp-hum, pm, water-level, thermocouple-mcp9601 | Default sensor-first cache, hide online, one decimal |
| motion | Detected/None labels for motion |
| airquality-airpressure | Three decimal places for VOC |
| ac-beko, ac-lg | Labels from the local IR protocol constants; Message/Received defaults |
| thermostat | Temperature only, two decimals with units, 10-second connection notice |
| ph | Default presentation plus the shared calibration overlay API |

Only the five devices with custom presentation have local `display_policy.h`
files. These contain meaningful policies and callbacks; there are no local
`display.cpp` implementations or forwarding `display.h` headers. The shared
library has no dependency on IRremoteESP8266 or any sensor driver.

Public functions include `display_update()`, `display_force_update()`,
`display_show_message()`, `display_set_connectivity_status()`, `display_sleep_for()`,
`display_set_overlay()` and `display_clear_overlay()`.
Four-line overlays take precedence over normal messages, connectivity rendering,
and the display button, preserving the pH calibration interaction. Temperature
control and pH calibration state machines remain outside this library.

## Build and Arduino IDE

Run CLI builds from the firmware repository root:

```sh
# Build a single firmware (e.g., thermostat for esp32)
arduino-cli compile --libraries ./libraries --fqbn esp32:esp32:esp32s3 ./thermostat
./build-all.sh --test-only
./build-all.sh --build-only
```

Both HomeAnthillCore and HomeAnthillDisplay are supplied by this checkout.
Install Adafruit GFX Library 1.12.6, Adafruit SSD1306 2.5.17, and their Adafruit
BusIO 1.17.4 dependency, plus the firmware-specific dependencies. These are build
requirements even when OLED hardware is disabled. Hardware is still optional.

For Arduino IDE, keep the existing sketchbook and create a link for each local
library once. From this repository's root, with the default macOS sketchbook:

```sh
mkdir -p "$HOME/Documents/Arduino/libraries"
ln -s "$PWD/libraries/HomeAnthillCore" "$HOME/Documents/Arduino/libraries/HomeAnthillCore"
ln -s "$PWD/libraries/HomeAnthillDisplay" "$HOME/Documents/Arduino/libraries/HomeAnthillDisplay"
```

If Core is already linked, add only the Display link. Inspect existing targets
before replacing them; adjust the destination for a different sketchbook path.
Restart the IDE, open the firmware sketch, select ESP32S3 Dev Module, and
Verify/Upload. Rebuilds use current source in the checkout without copying it.

## Verification

Every firmware's existing display suite now compiles this shared engine with
its actual presentation policy and SDK mocks. Their assertions preserve button,
power, rotation, precision, protocol labels, calibration overlays, and thermostat
connectivity behavior. The library tests also cover reinitialization,
fixed driver wiring, disabled operation, and missing-bus failure.

```sh
cmake -S libraries/HomeAnthillDisplay/tests -B libraries/HomeAnthillDisplay/tests/build
cmake --build libraries/HomeAnthillDisplay/tests/build --parallel
ctest --test-dir libraries/HomeAnthillDisplay/tests/build --output-on-failure
```

Host tests use C++17, GoogleTest 1.14.0, ArduinoJson 7.4.2, and the repository's
SDK mocks. The aggregate script and CI run these tests alongside all firmware
suites. Real OLED/I2C/button operation still needs hardware verification.
