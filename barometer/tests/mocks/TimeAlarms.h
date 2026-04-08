#pragma once

// ---------------------------------------------------------------------------
// TimeAlarms mock for host-side (native) unit test compilation.
// Provides AlarmID_t, TimeAlarmsClass, and the global Alarm instance.
// All methods are no-ops; the timer callbacks are never fired on the host.
// ---------------------------------------------------------------------------

#include <cstdint>

using AlarmID_t = uint8_t;

class TimeAlarmsClass {
public:
  // Register a repeating timer and return a dummy alarm ID.
  AlarmID_t timerRepeat(unsigned long /*period*/, void (* /*callback*/)()) { return 0; }

  void disable(AlarmID_t /*id*/) {}
  void enable(AlarmID_t /*id*/)  {}

  // Alarm.delay() is the TimeAlarms-aware equivalent of delay().
  void delay(unsigned long /*ms*/) {}
};

inline TimeAlarmsClass Alarm;
