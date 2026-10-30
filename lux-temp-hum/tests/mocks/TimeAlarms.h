#pragma once

// ---------------------------------------------------------------------------
// TimeAlarms mock for host-side (native) unit test compilation.
// Provides AlarmID_t, TimeAlarmsClass, and the global Alarm instance.
// Timer callbacks are never fired on the host, but enable/disable state is
// tracked so tests can assert alarm lifecycle behavior.
// ---------------------------------------------------------------------------

#include <cstdint>

using AlarmID_t = uint8_t;

class TimeAlarmsClass {
public:
  void reset() {
    next_id = 0;
    for (bool& value : enabled) {
      value = false;
    }
  }

  AlarmID_t timerRepeat(unsigned long /*period*/, void (* /*callback*/)()) {
    AlarmID_t id = next_id++;
    enabled[id] = true;
    return id;
  }

  void disable(AlarmID_t id) { enabled[id] = false; }
  void enable(AlarmID_t id)  { enabled[id] = true; }
  bool isEnabled(AlarmID_t id) const { return enabled[id]; }

  // Alarm.delay() is the TimeAlarms-aware equivalent of delay().
  void delay(unsigned long /*ms*/) {}

private:
  AlarmID_t next_id{0};
  bool enabled[16] = {};
};

inline TimeAlarmsClass Alarm;
