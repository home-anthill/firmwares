# HomeAnthillCore

Shared Arduino infrastructure for the ten home-anthill ESP32-S3 firmwares.
This is a source library kept in the firmware repository. Editing it and
rebuilding a consuming firmware uses the current source without installation,
publishing, or a version bump. Devices need the rebuilt firmware uploaded.

## Build with Arduino CLI

From the repository root:

```sh
# Build a single firmware (e.g., lux-temp-hum for esp32)
arduino-cli compile --libraries ./libraries --fqbn esp32:esp32:esp32s3 ./lux-temp-hum
./build-all.sh --build-only
./build-all.sh --test-only
```

The aggregate script and GitHub Actions already supply the library search path.
Install ArduinoJson 7.4.3 and PubSubClient 2.8.0 plus the firmware-specific
libraries listed in each device's README. ESP32 board support supplies WiFi,
HTTPClient, NetworkClientSecure, Preferences, and mbedTLS. No OLED, sensor, or
IR library is required by HomeAnthillCore itself.

## Arduino IDE

Keep the existing sketchbook location so installed libraries remain available.
Create a symbolic link for this library inside that sketchbook's `libraries`
folder. For the default macOS sketchbook and this checkout, run once:

```sh
mkdir -p "$HOME/Documents/Arduino/libraries"
ln -s /Users/ks89/git/home-anthill/firmwares/libraries/HomeAnthillCore \
  "$HOME/Documents/Arduino/libraries/HomeAnthillCore"
```

Check the actual sketchbook path in Arduino IDE Settings/Preferences and adjust
these paths for another machine. If `HomeAnthillCore` already exists there,
inspect it before replacing it. Restart the IDE after creating the link, open
one of the firmware sketches, select ESP32-S3, then Verify/Upload as usual.
The link points to the repository source; a ZIP installation would create an
independent copy and would not track local edits. The link is local machine
setup and is not committed to Git.

Alternatively, use the repository root as the IDE sketchbook, which makes its
`libraries` folder discoverable directly but changes the location used for
Library Manager installations.

## API and configuration boundary

Include `<HomeAnthillCore.h>`. The library never includes a sketch's `config.h`
or `secrets.h`. An optional shared header, `<HomeAnthillConfig.h>`, declares the
`wifi_config`, `mqtt_config`, and `registration_config` objects from macros
already defined by the sketch. Include it after the local settings:

```cpp
#include "config.h"
#include "secrets.h"
#include <HomeAnthillConfig.h>
```

The library's implementation and `<HomeAnthillCore.h>` do not include this
optional header. Applications can also construct the configuration structs
explicitly. Each sketch owns the plain/TLS client selected by `SSL` and calls
the library directly, for example:

```cpp
ha::wifi_connect(wifi_config, mac_address);
ha::mqtt_init(wifi_client, mqtt_callback, mqtt_config);
ha::mqtt_notify_value(device_uuid, feature_uuid, "temperature", value, mqtt_config);
```

There are no firmware-local forwarding headers or compatibility functions.
Configuration string pointers must remain valid for operations that use them
(the existing macros are string literals).

| Module | Shared behavior | Firmware responsibility |
|---|---|---|
| `anthill/wifi.h` | CA certificate, time synchronization, WiFi operations and legacy blocking retries | Credentials, client/TLS selection, when to connect |
| `anthill/registration.h` | HTTP payload, identity validation, persistence, optional network-error retries | Feature definitions, endpoint/identity, retry policy |
| `anthill/mqtt.h` | MQTT connection, subscriptions, signed values/alarms, disconnect on publish failure | Credentials, callback dispatch, numeric representation and topic aliases |
| `anthill/signing.h` | HMAC-SHA256 and nonce generation | Command identity/feature validation and nonce replay cache |
| `anthill/storage.h` | UUID, feature definitions, controller values in Preferences | Device-specific calibration/configuration rules |
| `anthill/feature_values.h` | Sensor-first feature cache and optional controller defaults | Whether controller defaults are enabled |

The legacy storage and feature-cache accessors and `mqtt_client` remain global
to preserve the existing firmware APIs. Network operations use the `ha`
namespace. The library currently represents one device and one MQTT connection
per firmware; it is not a multi-device client framework.

Blocking WiFi/MQTT functions preserve the ordinary firmware retry/restart
behavior. The thermostat uses `wifi_start_connect`, `mqtt_try_connect_once`,
and registration with `retry_network_errors=false` from its local connectivity
state machine. Single-attempt HTTP/MQTT operations can still block during I/O;
these functions do not make the network stack asynchronous. The thermostat's
control loop, output initialization, alarms, and cooldown policy remain local.

Existing contracts are preserved:

- Preferences namespace `device`, keys `uuid`, `features`, `numfeatures`, and
  `featureValues`, including complete-array controller-state replacement.
- HMAC-SHA256 canonical input:
  `deviceUuid\nfeatureUuid\nfeatureName\ntimestamp\nnonce\npayloadJson`.
- Motion/level integer payload formatting is selected explicitly at publish calls.
  PM topic aliases (`voc-index`, `nox-index`) and MCP9601 `temperature-status`
  do not change the registered feature name used for signing.
- Failed publishes disconnect MQTT. Sketch loops also retain their existing
  disconnect-on-failed-poll behavior.
- Controller command validation and replay protection remain in each sketch.

Sensor drivers, thermostat control and safety, pH calibration, IR dispatch,
displays, feature definitions, and both ESP-IDF simulators are outside this
library.

## Host verification

Each firmware's existing CMake/CTest suite compiles the shared source directly
against its local mocks, so device-specific expectations still exercise the
library. `./build-all.sh --test-only` runs all of them and the library tests.
The library also has configuration-injection tests independent of sketch
credentials:

```sh
cmake -S libraries/HomeAnthillCore/tests -B libraries/HomeAnthillCore/tests/build
cmake --build libraries/HomeAnthillCore/tests/build --parallel
ctest --test-dir libraries/HomeAnthillCore/tests/build --output-on-failure
```

Host tests require CMake and a C++17 compiler; first configuration fetches
GoogleTest 1.14.0 and ArduinoJson 7.4.2. The deterministic host signing branch
is test-only; ESP32 builds use mbedTLS and ESP32 entropy. Hardware/network
integration still needs validation on the devices.
