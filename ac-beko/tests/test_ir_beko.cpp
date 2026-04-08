#include <gtest/gtest.h>
#include <cstring>
#include <string>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "IRremoteESP8266.h"
#include "IRac.h"
#include "IRsend.h"
#include "ir_Coolix.h"

#include <ArduinoJson.h>
#include "secrets.h"

// NOTE: ir_beko.h and ir_beko.h (the mock) differ only in case.  On macOS's
// case-insensitive filesystem they resolve to the same file, so including
// "ir_beko.h" here would silently pull in the mock again and leave
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

class IrBekoTest : public ::testing::Test {
protected:
  void SetUp() override { IrCoolixMockState::reset(); }
};

// ===========================================================================
// JSON parsing
// ===========================================================================

TEST_F(IrBekoTest, MalformedJsonDoesNothing) {
  std::string bad = "not-json{{{";
  sendCommand(bad);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, EmptyArrayCallsSendOnce) {
  // No features to process — ir_send_signal() at the end still fires.
  sendCommand("[]");
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Security validation — apiToken / model mismatch
// ===========================================================================

TEST_F(IrBekoTest, ApiTokenMismatchCausesEarlyReturn) {
  std::string payload =
    R"([{"apiToken":"wrong-token","deviceUuid":"d","mac":"m","model":"ac-beko","featureUuid":"f","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, ModelMismatchCausesEarlyReturn) {
  std::string payload =
    std::string(R"([{"apiToken":")") + API_TOKEN + R"(","deviceUuid":"d","mac":"m","model":"wrong-model","featureUuid":"f","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, NullRequiredFieldsSkipsEntryAndStillSends) {
  // apiToken is missing — the entry should be skipped (continue), and the
  // outer ir_send_signal() at the end of the loop should still fire.
  std::string payload =
    std::string(R"([{"deviceUuid":"d","mac":"m","model":")") + MODEL + R"(","featureUuid":"f","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "on"
// ===========================================================================

TEST_F(IrBekoTest, OnValueOneCallsAcOnThenSend) {
  sendCommand(validPayload("on", "1"));

  EXPECT_TRUE(IrCoolixMockState::instance().on_called);
  EXPECT_FALSE(IrCoolixMockState::instance().off_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, OnValueZeroCallsAcOffThenSendAndReturnsEarly) {
  // "on"=0 triggers the early-return path: off() + ir_send_signal() + return.
  // The outer ir_send_signal() must NOT fire a second time.
  sendCommand(validPayload("on", "0"));

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_TRUE(IrCoolixMockState::instance().off_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, OnValueZeroEarlyReturnSkipsRemainingFeatures) {
  // Array: [on=0, setpoint=22] — after "on"=0 the function returns, so
  // setpoint must NOT be applied.
  std::string payload = "[" + validEntry("on", "0", "f1") + "," + validEntry("setpoint", "22", "f2") + "]";
  sendCommand(payload);

  EXPECT_TRUE(IrCoolixMockState::instance().off_called);
  EXPECT_FALSE(IrCoolixMockState::instance().settemp_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "setpoint"
// ===========================================================================

TEST_F(IrBekoTest, SetpointInRangeCallsSetTemp) {
  sendCommand(validPayload("setpoint", "22"));
  EXPECT_TRUE(IrCoolixMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrCoolixMockState::instance().last_temp, 22.0f);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, SetpointAtMinBoundaryCallsSetTemp) {
  sendCommand(validPayload("setpoint", "17"));  // TEMP_MIN = 17
  EXPECT_TRUE(IrCoolixMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrCoolixMockState::instance().last_temp, 17.0f);
}

TEST_F(IrBekoTest, SetpointAtMaxBoundaryCallsSetTemp) {
  sendCommand(validPayload("setpoint", "30"));  // TEMP_MAX = kCoolixMaxTemp = 30
  EXPECT_TRUE(IrCoolixMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrCoolixMockState::instance().last_temp, 30.0f);
}

TEST_F(IrBekoTest, SetpointBelowMinIsSkipped) {
  sendCommand(validPayload("setpoint", "16"));
  EXPECT_FALSE(IrCoolixMockState::instance().settemp_called);
  // Outer ir_send_signal() still fires (entry was skipped via continue).
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, SetpointAboveMaxIsSkipped) {
  sendCommand(validPayload("setpoint", "31"));
  EXPECT_FALSE(IrCoolixMockState::instance().settemp_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "mode"
// ===========================================================================

TEST_F(IrBekoTest, ModeOneSetsModeCool) {
  sendCommand(validPayload("mode", "1"));
  EXPECT_TRUE(IrCoolixMockState::instance().setmode_called);
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixCool));
}

TEST_F(IrBekoTest, ModeTwoSetsModeAuto) {
  sendCommand(validPayload("mode", "2"));
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixAuto));
}

TEST_F(IrBekoTest, ModeThreeSetsModeHeat) {
  sendCommand(validPayload("mode", "3"));
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixHeat));
}

TEST_F(IrBekoTest, ModeFourSetsModeFan) {
  sendCommand(validPayload("mode", "4"));
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixFan));
}

TEST_F(IrBekoTest, ModeFiveSetsModeDry) {
  sendCommand(validPayload("mode", "5"));
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixDry));
}

TEST_F(IrBekoTest, ModeUnsupportedValueDoesNotCallSetMode) {
  sendCommand(validPayload("mode", "99"));
  EXPECT_FALSE(IrCoolixMockState::instance().setmode_called);
  // send still fires
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "fanSpeed"
// ===========================================================================

TEST_F(IrBekoTest, FanSpeedFourSetsMinFan) {
  sendCommand(validPayload("fanSpeed", "1"));
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanMin));
}

TEST_F(IrBekoTest, FanSpeedThreeSetsMedFan) {
  sendCommand(validPayload("fanSpeed", "2"));
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanMed));
}

TEST_F(IrBekoTest, FanSpeedTwoSetsMaxFan) {
  sendCommand(validPayload("fanSpeed", "3"));
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanMax));
}

TEST_F(IrBekoTest, FanSpeedFourSetsAutoFan) {
  sendCommand(validPayload("fanSpeed", "4"));
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanAuto));
}

TEST_F(IrBekoTest, FanSpeedOneSetAuto0Fan) {
  sendCommand(validPayload("fanSpeed", "5"));
  EXPECT_TRUE(IrCoolixMockState::instance().setfan_called);
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanAuto0));
}

TEST_F(IrBekoTest, FanSpeedFiveIsNotSupported) {
  // cmd=6 hits the "Auto0 not supported" branch — setFan must NOT be called.
  sendCommand(validPayload("fanSpeed", "6"));
  EXPECT_FALSE(IrCoolixMockState::instance().setfan_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, FanSpeedUnsupportedValueDoesNotCallSetFan) {
  sendCommand(validPayload("fanSpeed", "99"));
  EXPECT_FALSE(IrCoolixMockState::instance().setfan_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Multi-feature payload
// ===========================================================================

TEST_F(IrBekoTest, MultipleFeaturesAllAppliedWithSingleSend) {
  // on=1, setpoint=24, mode=1(Cool), fanSpeed=2(Med) — all in one message.
  std::string payload = "[" +
    validEntry("on",       "1",  "f1") + "," +
    validEntry("setpoint", "24", "f2") + "," +
    validEntry("mode",     "1",  "f3") + "," +
    validEntry("fanSpeed", "2",  "f4") + "]";
  sendCommand(payload);

  EXPECT_TRUE(IrCoolixMockState::instance().on_called);
  EXPECT_TRUE(IrCoolixMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrCoolixMockState::instance().last_temp, 24.0f);
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixCool));
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanMed));
  // Only one send() call — via the outer ir_send_signal() at the end.
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}
