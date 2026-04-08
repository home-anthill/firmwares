#pragma once

// ---------------------------------------------------------------------------
// IRremoteESP8266 LG protocol mock for host-side (native) unit test
// compilation.
//
// The singleton IrLgMockState captures every call made to the IRLgAc
// object so tests can assert which AC state changes and IR transmissions
// occurred without real hardware or GPIO access.
//
// Usage in tests:
//   IrLgMockState::reset();
//   // ... call ir_send_command() ...
//   EXPECT_TRUE(IrLgMockState::instance().on_called);
//   EXPECT_EQ(IrLgMockState::instance().send_count, 1);
// ---------------------------------------------------------------------------

#include <cstdint>

// ---------------------------------------------------------------------------
// LG AC constants (values match the real IRremoteESP8266 library).
// ---------------------------------------------------------------------------

// Temperature range
constexpr uint8_t kLgAcMinTemp = 16;
constexpr uint8_t kLgAcMaxTemp = 30;

// Mode constants
constexpr uint8_t kLgAcCool = 0;
constexpr uint8_t kLgAcDry  = 1;
constexpr uint8_t kLgAcFan  = 2;
constexpr uint8_t kLgAcAuto = 3;
constexpr uint8_t kLgAcHeat = 4;

// Fan speed constants
constexpr uint8_t kLgAcFanLowest = 0;
constexpr uint8_t kLgAcFanMedium = 2;
constexpr uint8_t kLgAcFanHigh   = 10;
constexpr uint8_t kLgAcFanAuto   = 5;

// ---------------------------------------------------------------------------
// Remote model enum (values match the real library).
// ---------------------------------------------------------------------------
enum lg_ac_remote_model_t : uint8_t {
  AKB74955603 = 1,
  AKB75215403 = 2,
};

// ---------------------------------------------------------------------------
// Singleton mock state — reset in test SetUp() for isolation.
// ---------------------------------------------------------------------------
struct IrLgMockState {
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

  static IrLgMockState& instance() {
    static IrLgMockState s;
    return s;
  }
  static void reset() { instance() = IrLgMockState{}; }
};

// ---------------------------------------------------------------------------
// IRLgAc mock — mirrors the real class interface used by ir_lg.cpp.
// ---------------------------------------------------------------------------
class IRLgAc {
public:
  explicit IRLgAc(uint16_t /*pin*/) {}

  void setModel(lg_ac_remote_model_t model) {
    IrLgMockState::instance().last_model = static_cast<int>(model);
  }

  void calibrate() { IrLgMockState::instance().calibrate_called = true; }
  void begin()     { IrLgMockState::instance().begin_called = true; }

  void on()  { IrLgMockState::instance().on_called  = true; }
  void off() { IrLgMockState::instance().off_called = true; }

  void setTemp(float temp) {
    IrLgMockState::instance().settemp_called = true;
    IrLgMockState::instance().last_temp      = temp;
  }

  void setMode(uint8_t mode) {
    IrLgMockState::instance().setmode_called = true;
    IrLgMockState::instance().last_mode      = static_cast<int>(mode);
  }

  void setFan(uint8_t fan) {
    IrLgMockState::instance().setfan_called = true;
    IrLgMockState::instance().last_fan      = static_cast<int>(fan);
  }

  void send() { IrLgMockState::instance().send_count++; }
};
