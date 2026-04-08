#include <gtest/gtest.h>
#include <cstring>
#include <string>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "IRremoteESP8266.h"
#include "IRac.h"
#include "IRsend.h"
#include "ir_LG.h"

#include <ArduinoJson.h>
#include "secrets.h"  // provides API_TOKEN, MODEL

// NOTE: ir_lg.h and ir_LG.h (the mock) differ only in case.  On macOS's
// case-insensitive filesystem they resolve to the same file, so including
// "ir_lg.h" here would silently pull in the mock again and leave
// ir_send_command undeclared.  Declare the two firmware functions directly
// instead.
void ir_init();
void ir_send_command(char* topic, uint8_t* payload, unsigned int length);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a single JSON object entry (no array brackets) with correct credentials.
static std::string validEntry(const char* featureName, const char* valueStr, const char* featureUuid = "f0") {
  std::string s = R"({"apiToken":")";
  s += API_TOKEN;
  s += R"(","deviceUuid":"dev-uuid-0000","mac":"aa:bb:cc:dd:ee:ff","model":")";
  s += MODEL;
  s += R"(","featureUuid":")";
  s += featureUuid;
  s += R"(","featureName":")";
  s += featureName;
  s += R"(","value":)";
  s += valueStr;
  s += "}";
  return s;
}

// Build a single-entry JSON array payload with the given featureName/value.
static std::string validPayload(const char* featureName, const char* valueStr) {
  return "[" + validEntry(featureName, valueStr) + "]";
}

// Invoke ir_send_command with a std::string payload.
static void sendCommand(const std::string& json) {
  auto* p = reinterpret_cast<uint8_t*>(const_cast<char*>(json.c_str()));
  ir_send_command(nullptr, p, static_cast<unsigned int>(json.size()));
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class IrLgTest : public ::testing::Test {
protected:
  void SetUp() override { IrLgMockState::reset(); }
};

// ===========================================================================
// JSON parsing
// ===========================================================================

TEST_F(IrLgTest, MalformedJsonDoesNothing) {
  std::string bad = "not-json{{{";
  sendCommand(bad);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, EmptyArrayCallsSendOnce) {
  // No features to process — ir_send_signal() at the end still fires.
  sendCommand("[]");
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Security validation — apiToken / model mismatch
// ===========================================================================

TEST_F(IrLgTest, ApiTokenMismatchCausesEarlyReturn) {
  std::string payload =
    R"([{"apiToken":"wrong-token","deviceUuid":"d","mac":"m","model":"ac-lg","featureUuid":"f","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, ModelMismatchCausesEarlyReturn) {
  std::string payload =
    std::string(R"([{"apiToken":")") + API_TOKEN + R"(","deviceUuid":"d","mac":"m","model":"wrong-model","featureUuid":"f","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, NullRequiredFieldsSkipsEntryAndStillSends) {
  // apiToken is missing — the entry should be skipped (continue), and the
  // outer ir_send_signal() at the end of the loop should still fire.
  std::string payload =
    std::string(R"([{"deviceUuid":"d","mac":"m","model":")") + MODEL + R"(","featureUuid":"f","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "on"
// ===========================================================================

TEST_F(IrLgTest, OnValueOneCallsAcOnThenSend) {
  sendCommand(validPayload("on", "1"));

  EXPECT_TRUE(IrLgMockState::instance().on_called);
  EXPECT_FALSE(IrLgMockState::instance().off_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, OnValueZeroCallsAcOffThenSendAndReturnsEarly) {
  // "on"=0 triggers the early-return path: off() + ir_send_signal() + return.
  // The outer ir_send_signal() must NOT fire a second time.
  sendCommand(validPayload("on", "0"));

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_TRUE(IrLgMockState::instance().off_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, OnValueZeroEarlyReturnSkipsRemainingFeatures) {
  // Array: [on=0, setpoint=22] — after "on"=0 the function returns, so
  // setpoint must NOT be applied.
  std::string payload = "[" + validEntry("on", "0", "f1") + "," + validEntry("setpoint", "22", "f2") + "]";
  sendCommand(payload);

  EXPECT_TRUE(IrLgMockState::instance().off_called);
  EXPECT_FALSE(IrLgMockState::instance().settemp_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "setpoint"
// ===========================================================================

TEST_F(IrLgTest, SetpointInRangeCallsSetTemp) {
  sendCommand(validPayload("setpoint", "22"));
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 22.0f);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, SetpointAtMinBoundaryCallsSetTemp) {
  sendCommand(validPayload("setpoint", "18"));  // TEMP_MIN = 18
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 18.0f);
}

TEST_F(IrLgTest, SetpointAtMaxBoundaryCallsSetTemp) {
  sendCommand(validPayload("setpoint", "30"));  // TEMP_MAX = kLgAcMaxTemp = 30
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 30.0f);
}

TEST_F(IrLgTest, SetpointBelowMinIsSkipped) {
  sendCommand(validPayload("setpoint", "17"));
  EXPECT_FALSE(IrLgMockState::instance().settemp_called);
  // Outer ir_send_signal() still fires (entry was skipped via continue).
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, SetpointAboveMaxIsSkipped) {
  sendCommand(validPayload("setpoint", "31"));
  EXPECT_FALSE(IrLgMockState::instance().settemp_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "mode"
// ===========================================================================

TEST_F(IrLgTest, ModeOneSetsModeCool) {
  sendCommand(validPayload("mode", "1"));
  EXPECT_TRUE(IrLgMockState::instance().setmode_called);
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcCool));
}

TEST_F(IrLgTest, ModeTwoSetsModeAuto) {
  sendCommand(validPayload("mode", "2"));
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcAuto));
}

TEST_F(IrLgTest, ModeThreeSetsModeHeat) {
  sendCommand(validPayload("mode", "3"));
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcHeat));
}

TEST_F(IrLgTest, ModeFourSetsModeFan) {
  sendCommand(validPayload("mode", "4"));
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcFan));
}

TEST_F(IrLgTest, ModeFiveSetsModeDry) {
  sendCommand(validPayload("mode", "5"));
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcDry));
}

TEST_F(IrLgTest, ModeUnsupportedValueDoesNotCallSetMode) {
  sendCommand(validPayload("mode", "99"));
  EXPECT_FALSE(IrLgMockState::instance().setmode_called);
  // send still fires
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "fanSpeed"
// ===========================================================================

TEST_F(IrLgTest, FanSpeedOneSetsMinFan) {
  sendCommand(validPayload("fanSpeed", "1"));
  EXPECT_TRUE(IrLgMockState::instance().setfan_called);
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanLowest));
}

TEST_F(IrLgTest, FanSpeedTwoSetsMedFan) {
  sendCommand(validPayload("fanSpeed", "2"));
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanMedium));
}

TEST_F(IrLgTest, FanSpeedThreeSetsMaxFan) {
  sendCommand(validPayload("fanSpeed", "3"));
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanHigh));
}

TEST_F(IrLgTest, FanSpeedFourSetsAutoFan) {
  sendCommand(validPayload("fanSpeed", "4"));
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanAuto));
}

TEST_F(IrLgTest, FanSpeedFiveIsNotSupported) {
  // cmd=5 hits the "Auto0 not supported" branch — setFan must NOT be called.
  sendCommand(validPayload("fanSpeed", "5"));
  EXPECT_FALSE(IrLgMockState::instance().setfan_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, FanSpeedUnsupportedValueDoesNotCallSetFan) {
  sendCommand(validPayload("fanSpeed", "99"));
  EXPECT_FALSE(IrLgMockState::instance().setfan_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Multi-feature payload
// ===========================================================================

TEST_F(IrLgTest, MultipleFeaturesAllAppliedWithSingleSend) {
  // on=1, setpoint=24, mode=1(Cool), fanSpeed=2(Med) — all in one message.
  std::string payload = "[" +
    validEntry("on",       "1",  "f1") + "," +
    validEntry("setpoint", "24", "f2") + "," +
    validEntry("mode",     "1",  "f3") + "," +
    validEntry("fanSpeed", "2",  "f4") + "]";
  sendCommand(payload);

  EXPECT_TRUE(IrLgMockState::instance().on_called);
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 24.0f);
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcCool));
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanMedium));
  // Only one send() call — via the outer ir_send_signal() at the end.
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}
