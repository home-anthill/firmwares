#pragma once

// ---------------------------------------------------------------------------
// IRremoteESP8266 Coolix protocol mock for host-side (native) unit test
// compilation.
//
// The singleton IrCoolixMockState captures every call made to the IRCoolixAC
// object so tests can assert which AC state changes and IR transmissions
// occurred without real hardware or GPIO access.
//
// Usage in tests:
//   IrCoolixMockState::reset();
//   // ... call ir_send_command() ...
//   EXPECT_TRUE(IrCoolixMockState::instance().on_called);
//   EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
// ---------------------------------------------------------------------------

#include <cstdint>

// ---------------------------------------------------------------------------
// Coolix AC constants (values match the real IRremoteESP8266 library).
// ---------------------------------------------------------------------------

// Temperature range
constexpr uint8_t kCoolixTempMin = 17;
constexpr uint8_t kCoolixTempMax = 30;

// Mode constants
constexpr uint8_t kCoolixCool = 0;
constexpr uint8_t kCoolixDry  = 1;
constexpr uint8_t kCoolixAuto = 2;
constexpr uint8_t kCoolixHeat = 3;
constexpr uint8_t kCoolixFan  = 4;

// Fan speed constants
constexpr uint8_t kCoolixFanAuto0 = 0;
constexpr uint8_t kCoolixFanMax = 1;
constexpr uint8_t kCoolixFanMed   = 2;
constexpr uint8_t kCoolixFanMin   = 4;
constexpr uint8_t kCoolixFanAuto   = 5;

// ---------------------------------------------------------------------------
// Singleton mock state — reset in test SetUp() for isolation.
// ---------------------------------------------------------------------------
struct IrCoolixMockState {
  // Method call flags
  bool on_called{false};
  bool off_called{false};
  bool calibrate_called{false};
  bool begin_called{false};

  // Last-set values (flags track whether the setter was called at all)
  bool  settemp_called{false};
  float last_temp{0.0f};

  bool setmode_called{false};
  int  last_mode{-1};

  bool setfan_called{false};
  int  last_fan{-1};

  int last_model{-1};

  // How many times send() (i.e. ir_send_signal) was called
  int send_count{0};

  static IrCoolixMockState& instance() {
    static IrCoolixMockState s;
    return s;
  }
  static void reset() { instance() = IrCoolixMockState{}; }
};

// ---------------------------------------------------------------------------
// IRCoolixAC mock — mirrors the real class interface used by ir_beko.cpp.
// ---------------------------------------------------------------------------
class IRCoolixAC {
public:
  explicit IRCoolixAC(uint16_t /*pin*/) {}

  void calibrate() { IrCoolixMockState::instance().calibrate_called = true; }
  void begin()     { IrCoolixMockState::instance().begin_called = true; }

  void on()  { IrCoolixMockState::instance().on_called  = true; }
  void off() { IrCoolixMockState::instance().off_called = true; }

  void setTemp(float temp) {
    IrCoolixMockState::instance().settemp_called = true;
    IrCoolixMockState::instance().last_temp      = temp;
  }

  void setMode(uint8_t mode) {
    IrCoolixMockState::instance().setmode_called = true;
    IrCoolixMockState::instance().last_mode      = static_cast<int>(mode);
  }

  void setFan(uint8_t fan) {
    IrCoolixMockState::instance().setfan_called = true;
    IrCoolixMockState::instance().last_fan      = static_cast<int>(fan);
  }

  void send() { IrCoolixMockState::instance().send_count++; }
};
