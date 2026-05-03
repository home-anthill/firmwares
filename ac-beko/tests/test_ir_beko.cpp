#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <ctime>

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
void ir_send_command(const char* saved_device_uuid, const char* saved_mac_address,
                     JsonArray saved_features, char* topic, uint8_t* payload,
                     unsigned int length);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a single JSON object entry (no array brackets) with a valid test signature.
static std::string validEntry(const char* featureName, const char* valueStr, const char* featureUuid = "f0") {
  std::string s = R"({"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":")";
  s += MODEL;
  s += R"(","featureUuid":")";
  s += featureUuid;
  s += R"(","featureName":")";
  s += featureName;
  s += R"(","timestamp":)";
  s += std::to_string(time(nullptr));
  s += R"(,"nonce":"00112233445566778899aabbccddeeff","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":)";
  s += valueStr;
  s += "}}";
  return s;
}

// Build a single-entry JSON array payload with the given featureName/value.
static std::string validPayload(const char* featureName, const char* valueStr) {
  return "[" + validEntry(featureName, valueStr) + "]";
}

static char g_dummy_uuid[37] = "device-uuid-test-0000-000000000000";
static char g_dummy_mac[18] = "aa:bb:cc:dd:ee:ff";
static JsonDocument g_dummy_doc;

// Invoke ir_send_command with a std::string payload.
static void sendCommand(const std::string& json) {
  g_dummy_doc.clear();
  JsonArray saved_features = g_dummy_doc.to<JsonArray>();
  JsonObject on_feature = saved_features.add<JsonObject>();
  on_feature["uuid"] = "f1";
  on_feature["name"] = "on";
  JsonObject setpoint_feature = saved_features.add<JsonObject>();
  setpoint_feature["uuid"] = "f2";
  setpoint_feature["name"] = "setpoint";
  JsonObject mode_feature = saved_features.add<JsonObject>();
  mode_feature["uuid"] = "f3";
  mode_feature["name"] = "mode";
  JsonObject fan_feature = saved_features.add<JsonObject>();
  fan_feature["uuid"] = "f4";
  fan_feature["name"] = "fanSpeed";
  auto* p = reinterpret_cast<uint8_t*>(const_cast<char*>(json.c_str()));
  ir_send_command(g_dummy_uuid, g_dummy_mac, saved_features, nullptr, p,
                  static_cast<unsigned int>(json.size()));
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
    R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":"ac-beko","featureUuid":"f1","featureName":"on","timestamp":1777630000,"nonce":"00112233445566778899aabbccddeeff","signature":"wrong-signature","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, ModelMismatchCausesEarlyReturn) {
  std::string payload =
    R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":"wrong-model","featureUuid":"f1","featureName":"on","timestamp":)" +
    std::to_string(time(nullptr)) +
    R"(,"nonce":"00112233445566778899aabbccddeeff","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, NullRequiredFieldsSkipsEntryAndStillSends) {
  // signature is missing — the entry should be skipped (continue), and the
  // outer ir_send_signal() at the end of the loop should still fire.
  std::string payload =
    std::string(R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":")") + MODEL + R"(","featureUuid":"f1","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, WrongDeviceUuidCausesEarlyReturn) {
  std::string payload =
      std::string(R"([{"deviceUuid":"wrong-device","mac":"aa:bb:cc:dd:ee:ff","model":")") +
      MODEL + R"(","featureUuid":"f1","featureName":"on","timestamp":)" +
      std::to_string(time(nullptr)) +
      R"(,"nonce":"00112233445566778899aabbccddeeff","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, WrongMacCausesEarlyReturn) {
  std::string payload =
      std::string(R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"11:22:33:44:55:66","model":")") +
      MODEL + R"(","featureUuid":"f1","featureName":"on","timestamp":)" +
      std::to_string(time(nullptr)) +
      R"(,"nonce":"00112233445566778899aabbccddeeff","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, WrongFeatureUuidCausesEarlyReturn) {
  sendCommand(validPayload("on", "1"));

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

TEST_F(IrBekoTest, FeatureUuidMismatchedToFeatureNameCausesEarlyReturn) {
  sendCommand("[" + validEntry("on", "1", "f2") + "]");

  EXPECT_FALSE(IrCoolixMockState::instance().on_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 0);
}

// ===========================================================================
// Feature: "on"
// ===========================================================================

TEST_F(IrBekoTest, OnValueOneCallsAcOnThenSend) {
  sendCommand("[" + validEntry("on", "1", "f1") + "]");

  EXPECT_TRUE(IrCoolixMockState::instance().on_called);
  EXPECT_FALSE(IrCoolixMockState::instance().off_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, OnValueZeroCallsAcOffThenSendAndReturnsEarly) {
  // "on"=0 triggers the early-return path: off() + ir_send_signal() + return.
  // The outer ir_send_signal() must NOT fire a second time.
  sendCommand("[" + validEntry("on", "0", "f1") + "]");

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
  sendCommand("[" + validEntry("setpoint", "22", "f2") + "]");
  EXPECT_TRUE(IrCoolixMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrCoolixMockState::instance().last_temp, 22.0f);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, SetpointAtMinBoundaryCallsSetTemp) {
  sendCommand("[" + validEntry("setpoint", "17", "f2") + "]");  // TEMP_MIN = 17
  EXPECT_TRUE(IrCoolixMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrCoolixMockState::instance().last_temp, 17.0f);
}

TEST_F(IrBekoTest, SetpointAtMaxBoundaryCallsSetTemp) {
  sendCommand("[" + validEntry("setpoint", "30", "f2") + "]");  // TEMP_MAX = kCoolixMaxTemp = 30
  EXPECT_TRUE(IrCoolixMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrCoolixMockState::instance().last_temp, 30.0f);
}

TEST_F(IrBekoTest, SetpointBelowMinIsSkipped) {
  sendCommand("[" + validEntry("setpoint", "16", "f2") + "]");
  EXPECT_FALSE(IrCoolixMockState::instance().settemp_called);
  // Outer ir_send_signal() still fires (entry was skipped via continue).
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, SetpointAboveMaxIsSkipped) {
  sendCommand("[" + validEntry("setpoint", "31", "f2") + "]");
  EXPECT_FALSE(IrCoolixMockState::instance().settemp_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "mode"
// ===========================================================================

TEST_F(IrBekoTest, ModeOneSetsModeCool) {
  sendCommand("[" + validEntry("mode", "1", "f3") + "]");
  EXPECT_TRUE(IrCoolixMockState::instance().setmode_called);
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixCool));
}

TEST_F(IrBekoTest, ModeTwoSetsModeAuto) {
  sendCommand("[" + validEntry("mode", "2", "f3") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixAuto));
}

TEST_F(IrBekoTest, ModeThreeSetsModeHeat) {
  sendCommand("[" + validEntry("mode", "3", "f3") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixHeat));
}

TEST_F(IrBekoTest, ModeFourSetsModeFan) {
  sendCommand("[" + validEntry("mode", "4", "f3") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixFan));
}

TEST_F(IrBekoTest, ModeFiveSetsModeDry) {
  sendCommand("[" + validEntry("mode", "5", "f3") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_mode, static_cast<int>(kCoolixDry));
}

TEST_F(IrBekoTest, ModeUnsupportedValueDoesNotCallSetMode) {
  sendCommand("[" + validEntry("mode", "99", "f3") + "]");
  EXPECT_FALSE(IrCoolixMockState::instance().setmode_called);
  // send still fires
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "fanSpeed"
// ===========================================================================

TEST_F(IrBekoTest, FanSpeedFourSetsMinFan) {
  sendCommand("[" + validEntry("fanSpeed", "1", "f4") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanMin));
}

TEST_F(IrBekoTest, FanSpeedThreeSetsMedFan) {
  sendCommand("[" + validEntry("fanSpeed", "2", "f4") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanMed));
}

TEST_F(IrBekoTest, FanSpeedTwoSetsMaxFan) {
  sendCommand("[" + validEntry("fanSpeed", "3", "f4") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanMax));
}

TEST_F(IrBekoTest, FanSpeedFourSetsAutoFan) {
  sendCommand("[" + validEntry("fanSpeed", "4", "f4") + "]");
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanAuto));
}

TEST_F(IrBekoTest, FanSpeedOneSetAuto0Fan) {
  sendCommand("[" + validEntry("fanSpeed", "5", "f4") + "]");
  EXPECT_TRUE(IrCoolixMockState::instance().setfan_called);
  EXPECT_EQ(IrCoolixMockState::instance().last_fan, static_cast<int>(kCoolixFanAuto0));
}

TEST_F(IrBekoTest, FanSpeedFiveIsNotSupported) {
  // cmd=6 hits the "Auto0 not supported" branch — setFan must NOT be called.
  sendCommand("[" + validEntry("fanSpeed", "6", "f4") + "]");
  EXPECT_FALSE(IrCoolixMockState::instance().setfan_called);
  EXPECT_EQ(IrCoolixMockState::instance().send_count, 1);
}

TEST_F(IrBekoTest, FanSpeedUnsupportedValueDoesNotCallSetFan) {
  sendCommand("[" + validEntry("fanSpeed", "99", "f4") + "]");
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
