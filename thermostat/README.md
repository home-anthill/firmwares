# Thermostat Output Control Analysis

## Scope and definitions

This document describes the output-control implementation in `thermostat.ino`,
configuration persistence in `controller.cpp`, sensor filtering in
`temp_sensor.cpp`, and output settings in `secrets.h`.

```text
S  = setpoint
T  = tolerance
Tl = S - T       lower boundary
Tu = S + T       upper boundary
t  = measured temperature

thermostat_mode:
  -1 = cooling fault
   0 = sleep/off
  +1 = cold
  +2 = heat
```

Current defaults and timing:

```text
S = 25 C
T = 5 C
Tl = 20 C
Tu = 30 C
temperature sample period = 5 seconds
fan turn-off delay = 240 seconds
cooling short rise check = 120 seconds
cooling wide rise check = 600 seconds
OPERATING_MODE = 1 (heating)
```

`OPERATING_MODE` is a compile-time configuration:

| Value | Selected control | Runtime outputs |
|---:|---|---|
| `0` | Cooling | COLD, PUMP, and FAN |
| `1` | Heating | HEAT only |

Any other value stops compilation with an error. Changing `OPERATING_MODE`
requires rebuilding and flashing the firmware; it is not an MQTT setting.

`OPERATING_MODE` and `thermostat_mode` are different concepts:

```text
OPERATING_MODE   = compile-time operating mode (0 cooling, 1 heating)
thermostat_mode  = runtime state (-1 fault, 0 sleep, 1 cold, 2 heat)
```

The runtime mode feature is registered and cached. Mode transitions are
published as signed sensor telemetry while MQTT is connected.

On a fresh Preferences/NVS store, `get_setpoint()` and `get_tolerance()` use
the defaults above. A normal USB firmware upload may leave existing NVS data
intact, so an already-used board can boot with its previously stored values.

## Physical output polarity

`output_level(active_low, active)` translates a logical command into a GPIO
level. With the current `secrets.h` configuration:

> [!WARNING]
> **Mandatory hardware safety requirement:** every HEAT, COLD, FAN, and PUMP
> control input must have an external resistor that holds it at its configured
> inactive level whenever the ESP32 GPIO is high-impedance. This includes reset,
> bootloader execution, USB flashing, startup, and loss of ESP32 power. An
> active-low output requires a pull-up; an active-high output requires a
> pull-down. Firmware initialization is defense in depth and cannot replace
> these external resistors. The current thermostat hardware implements this
> requirement.

| Output | Active low | Logical OFF | Logical ON |
|---|---:|---:|---:|
| HEAT | false | LOW | HIGH |
| COLD | true | HIGH | LOW |
| FAN | false | LOW | HIGH |
| PUMP | false | LOW | HIGH |

The COLD relay is different from the others: a LOW pin activates it.

| Output | Active state | Required external bias | Safe reset level |
|---|---:|---:|---:|
| HEAT | HIGH | Pull-down | LOW |
| COLD | LOW | Pull-up | HIGH |
| FAN | HIGH | Pull-down | LOW |
| PUMP | HIGH | Pull-down | LOW |

If any `*_ACTIVE_LOW` setting changes, the corresponding hardware bias must be
reviewed and inverted when necessary. Resistor values and voltage rails must
match the actual relay/driver input circuit and must not back-power the ESP32.

## Startup output safety

`outputs_init()` is the first hardware action in `setup()`. For every output it
preloads the inactive ESP32 output latch, enables the output driver, and then
reinforces the inactive level through `digitalWrite()`:

```text
gpio_set_level(pin, OFF)
          |
          v
pinMode(pin, OUTPUT)
          |
          v
digitalWrite(pin, OFF)
```

All four outputs are initialized OFF regardless of `OPERATING_MODE`. After the
first valid temperature read, only the selected operating mode is written:

```text
OPERATING_MODE=0                  OPERATING_MODE=1

COLD  <- control loop             HEAT <- control loop
PUMP  <- control loop
FAN   <- control loop

HEAT remains at startup OFF       COLD/PUMP/FAN remain at startup OFF
```

## Cooling control (`OPERATING_MODE=0`)

Heating comparisons and runtime HEAT writes are not compiled into this branch (if you configure `OPERATING_MODE=0` in `secrets.h`).

### First valid reading

There is no mode history after boot. The first valid reading starts cooling as
soon as the measured temperature is above the setpoint:

```text
Temperature increases from left to right.
`^` marks the current measured temperature `t`.
```

```text
If t > S it starts cooling

       SLEEP                      COLD
---------------------|=====^==============>
                     S     t

SLEEP: t <= S
COLD:  t > S
```

```text
If t <= S it's sleeping

       SLEEP                      COLD
----------^----------|-------------------->
          t          S

SLEEP: t <= S
COLD:  t > S
```

Starting cooling sets both state variables to cooling:

```text
thermostat_mode      = 1
prev_thermostat_mode = 1
```

### Active cycle and restart deadband

Cooling continues while `t > S` and stops when `t <= S`. The previous mode
remains `1`, so cooling cannot restart until `t >= Tu`:

```text
Temperature increases from left to right (`--->`).
`^` marks the current measured temperature `t`.
`|` marks the current setpoint `S`.
`*` marks the current upper boundary `Tu`.

After cooling has stopped, S = 20 C and Tu = 25 C:
```

```text
Case 1: t = 22 C, still below Tu

S = 20 C      t = 22 C                Tu = 25 C
--|-------------^------------------------*--------------->
              SLEEP: COLD remains OFF
```

```text
Case 2: t = 24 C, close to Tu but still below it

S = 20 C             t = 24 C         Tu = 25 C
--|--------------------^-----------------*--------------->
                            SLEEP: COLD remains OFF
```

```text
Case 3: t = 25 C, exactly at Tu

S = 20 C                           t = Tu = 25 C
--|----------------------------------^------------------->
                                   COLD starts (ON)
```

```text
Case 4: t = 27 C, above Tu

S = 20 C                          Tu = 25 C   t = 27 C
--|----------------------------------|----------^-------->
                                               COLD starts (ON)
```

Once COLD has started, `Tu` is no longer the stop boundary. Cooling remains
active while the temperature falls and stops only when `t <= S`:

```text
Case 5: COLD is already active and t has fallen to 22 C

S = 20 C      t = 22 C                    Tu = 25 C
--|-------------^----------------------------|----------->
              COLD remains ON
```

```text
Case 6: COLD is already active and t reaches S

t = S = 20 C                               Tu = 25 C
--^------------------------------------------|----------->
COLD turns OFF; mode becomes SLEEP
```

The two state transitions are therefore:

```text
SLEEP -- t >= Tu --> COLD
COLD  -- t <= S  --> SLEEP
```

The complete cycle is:

```text
t > S and no cooling history  -> start cooling
cooling and t > S             -> keep cooling
cooling and t <= S            -> stop at setpoint
sleep with cooling history    -> wait while t < Tu
t >= Tu                       -> restart cooling
```

When cooling is active, the output requests are:

| COLD | PUMP | FAN | HEAT |
|---:|---:|---:|---:|
| ON | ON | ON | untouched; remains OFF from startup |

When cooling is sleeping, COLD and PUMP are OFF. FAN is requested OFF and may
stay physically on until its cooldown deadline.

### Cooling example

With `S = 20 C`, `T = 5 C`, and `Tu = 25 C`:

The table shows the state used to process each new temperature sample:

```text
State at start  = what the thermostat is doing when the sample arrives
Remembered mode = hysteresis memory; the last active mode that was started
Decision        = action selected after evaluating this sample
State after     = state used by the next sample
```

The remembered mode is meaningful because it is not cleared when COLD stops at
the setpoint. It tells the next sleeping sample to use the restart boundary
`Tu`, rather than the first-start boundary `S`.

```text
Sample  t      State at start  Remembered mode  Decision                 State after
------  -----  --------------  ---------------  -----------------------  -----------
1       22 C   SLEEP           none             first start above S      COLD
2       22 C   COLD            COLD             continue cooling         COLD
3       20 C   COLD            COLD             stop at setpoint S       SLEEP
4       22 C   SLEEP           COLD             wait; still below Tu     SLEEP
5       24 C   SLEEP           COLD             wait; still below Tu     SLEEP
6       25 C   SLEEP           COLD             restart exactly at Tu    COLD
```

The repeated `22 C` samples demonstrate why both state values matter:

```text
t = 22 C, State=SLEEP, Remembered=none  -> first start: COLD turns ON
t = 22 C, State=COLD,  Remembered=COLD  -> active cycle: COLD stays ON
t = 22 C, State=SLEEP, Remembered=COLD  -> restart cycle: COLD stays OFF
```

### Cooling rise safety checks

The COLD output is hardware-agnostic and may be connected to a conventional
cooler or a Peltier module. A reversed or damaged Peltier system can heat the
controlled fluid while cooling is requested, so cooling builds perform two
simple temperature-rise checks while mode is `1` (cold):

```text
COLD starts at t0
  |
  +-- after 120 s: current temperature > temperature at t0?
  |                     yes -> mode = -1 (cooling fault)
  |
  +-- every 600 s: current temperature > wide-window start temperature?
                        yes -> mode = -1 (cooling fault)
```

These are safety checks against unwanted heating, not cooling-performance
checks. An equal or lower temperature passes; no minimum decrease is required.
The comparison is strict: any measured increase above the baseline triggers a
fault; there is no configured temperature-rise allowance.

The short check runs once per continuous COLD cycle and compares against the
temperature captured when COLD started. The wide check starts from that same
initial temperature and repeats for as long as COLD remains active. After every
successful wide check, its current temperature and time become the baseline for
the next wide window.

Both monitors reset when cooling stops normally at the setpoint. A later
hysteresis restart begins new short and wide windows with a new baseline.
Checks run only when a valid temperature sample is processed. With a 5-second
sample period, a deadline is normally evaluated at the first sample at or after
that deadline, up to about 5 seconds later.

The cooling fault is latched until reboot:

```text
mode = -1 (cooling fault)
COLD = OFF immediately
PUMP = OFF immediately
FAN  = requested OFF using its configured cooldown delay
```

When the fault transition occurs while MQTT is connected, the thermostat also
publishes a signed real-time alarm on
`alarms/{deviceUuid}/features/{modeFeatureUuid}/thermostat-mode-error` with
`{"value":-1.0}`. If MQTT is disconnected at that moment, the alarm is skipped
and is not queued for delivery after reconnection.

The two durations are configured by `COOLING_SHORT_RISE_CHECK_SECONDS` and
`COOLING_WIDE_RISE_CHECK_SECONDS` in `secrets.h`. Both values must be greater
than zero or firmware compilation fails.

## Heating control (`OPERATING_MODE=1`)

Cooling comparisons and runtime COLD, PUMP, and FAN writes are not compiled
into this branch (if you configure `OPERATING_MODE=1` in `secrets.h`).

### First valid reading

There is no mode history after boot. The first valid reading starts heating as
soon as the measured temperature is below the setpoint:

```text
If t < S it starts heating

       HEAT                      SLEEP
====================^-----|-------------->
                    t     S

HEAT:  t < S
SLEEP: t >= S
```

```text
If t >= S it's sleeping

        HEAT                      SLEEP
---------|----------^-------------------->
         S          t

HEAT:  t < S
SLEEP: t >= S
```

Starting heating sets both state variables to heating:

```text
thermostat_mode      = 2
prev_thermostat_mode = 2
```

### Active cycle and restart deadband

Heating continues while `t < S` and stops when `t >= S`. The previous mode
remains `2`, so heating cannot restart until `t <= Tl`:

The complete cycle is:

```text
t < S and no heating history  -> start heating
heating and t < S             -> keep heating
heating and t >= S            -> stop at setpoint
sleep with heating history    -> wait while t > Tl
t <= Tl                       -> restart heating
```

When heating is active, the output requests are:

| HEAT | COLD | PUMP | FAN |
|---:|---:|---:|---:|
| ON | untouched; remains OFF from startup | untouched; remains OFF from startup | untouched; remains OFF from startup |

When heating is sleeping, HEAT is OFF.

### Heating example

With `S = 20 C`, `T = 5 C`, and `Tl = 15 C`:

```text
Sample  t      State at start  Remembered mode  Decision                 State after
------  -----  --------------  ---------------  -----------------------  -----------
1       18 C   SLEEP           none             first start below S      HEAT
2       18 C   HEAT            HEAT             continue heating         HEAT
3       20 C   HEAT            HEAT             stop at setpoint S       SLEEP
4       18 C   SLEEP           HEAT             wait; still above Tl     SLEEP
5       16 C   SLEEP           HEAT             wait; still above Tl     SLEEP
6       15 C   SLEEP           HEAT             restart exactly at Tl    HEAT
```

## Fan turn-off delay

This applies only to cooling builds. While cooling, every temperature cycle
calls `write_fan_output(true)`, which turns FAN on and clears an old deadline.

When cooling stops, the first `write_fan_output(false)` call does this:

```text
fan_requested_active: true -> false
fan_turn_off_at_ms = now + 240 seconds
FAN remains physically ON
```

Later valid temperature cycles check the deadline:

```text
before deadline: FAN stays ON
at/after deadline: FAN turns OFF
```

Because temperature is sampled every 5 seconds, normal fan shutdown can occur
up to about one sample period after the 240-second deadline.

## Startup and reboot sequence

```text
MCU reset
  |
  +-- external pull resistors hold every control input inactive
  +-- state initializes to SLEEP / no previous mode
  +-- setup(): preload and enable all outputs at inactive levels
  +-- setup(): delay 1 second
  +-- initialize MCP9600 temperature sensor
  +-- initialize display
  +-- read saved feature definitions and display values
  +-- create and enable the 5-second temperature alarm
  +-- start the non-blocking WiFi/MQTT state machine
  +-- first valid temperature control call
      +-- normally from the 5-second alarm, or
      +-- earlier if MQTT connects and calls publish_initial_values()
```

### Fresh board or erased NVS

There are no saved controller values, so control uses `S = 25 C`, `T = 5 C`,
sleep mode, and no history. This is expected and is not an issue: tolerance is a
restart deadband, and no operating mode has run yet.

The first finite MCP9600 reading uses only the selected operating mode:

```text
OPERATING_MODE=0: start cold mode for t > 25 C; otherwise stay asleep
OPERATING_MODE=1: start heat mode for t < 25 C; otherwise stay asleep
```

### Reboot with existing NVS

Saved setpoint and tolerance can be restored, but operating state and history
are not persisted. Every reboot deliberately starts sleeping with no history,
then bootstraps the selected operating mode from the first valid temperature
reading.

### USB firmware upload

Uploading a sketch does not necessarily erase Preferences/NVS:

```text
erased NVS       -> defaults 25 C / 5 C
retained NVS     -> previously stored values, if readable
```

For deterministic commissioning, deployment must explicitly decide whether to
preserve or erase Preferences.

### Controller command contract

Every thermostat configuration command contains the complete controller state,
including both setpoint and tolerance. `set_configuration()` therefore replaces
the persisted `featureValues` array with the received array by design; partial
configuration commands are not part of the protocol.

## Prioritized findings

1. **Critical: sensor failure can leave the selected outputs energized
   indefinitely.** `read_temp_sensor_value()` only logs when it receives `NaN`;
   it does not turn outputs off. Also, `temp_get_temperature()` returns the last
   accepted value after rejecting a later bad reading, with no maximum age or
   consecutive-failure limit. A disconnected, frozen, or noisy sensor can keep
   control operating from stale data. The cooling rise checks do not solve a
   frozen sensor because an unchanged stale value appears flat and therefore
   passes by design. A pending fan deadline is also not serviced while readings
   remain `NaN`, so FAN can remain on.

## Test coverage and current result

The host suite builds `thermostat.ino` twice:

```text
test_main_ino       -> OPERATING_MODE=0 (cooling)
test_main_ino_heat  -> OPERATING_MODE=1 (heating)
```

The complete eight-suite host test set includes:

```text
storage_tests
registration_tests
mqtt_handler_tests
display_tests
temp_sensor_tests
controller_tests
main_ino_tests
main_ino_heat_tests
```

All eight suites pass with the current implementation.

The cooling and heating tests verify first start, selected output activation,
absence of runtime writes to unselected outputs, stop at the setpoint, sleep
samples inside the deadband, and restart exactly at `Tu` or `Tl`. Cooling tests
also verify that flat temperature passes both safety checks, a rise latches
mode `-1`, COLD and PUMP stop, and FAN completes its cooldown.

The setpoint tests and registration specification use a maximum of `35 C`:
`35 C` is accepted at the boundary and `36 C` is rejected.

Important missing tests:

1. `NaN` while HEAT is active and while COLD/PUMP/FAN are active.
2. Rejected or stale sensor values for longer than a safe timeout.
3. Fan cooldown expiry when later temperature reads fail.
