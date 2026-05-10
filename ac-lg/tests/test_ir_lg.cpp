#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <ctime>

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
void ir_send_command(const char* saved_device_uuid, const char* saved_mac_address,
                     JsonArray saved_features, char* topic, uint8_t* payload,
                     unsigned int length);
void reset_command_nonce_cache_for_test();

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a single JSON object entry (no array brackets) with a valid test signature.
static std::string validEntry(const char* featureName, const char* valueStr, const char* featureUuid = "f0",
                              const char* nonce = "00112233445566778899aabbccddeeff") {
  std::string s = R"({"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":")";
  s += MODEL;
  s += R"(","featureUuid":")";
  s += featureUuid;
  s += R"(","featureName":")";
  s += featureName;
  s += R"(","timestamp":)";
  s += std::to_string(time(nullptr));
  s += R"(,"nonce":")";
  s += nonce;
  s += R"(","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":)";
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

class IrLgTest : public ::testing::Test {
protected:
  void SetUp() override {
    IrLgMockState::reset();
    reset_command_nonce_cache_for_test();
  }
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
    R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":"ac-lg","featureUuid":"f1","featureName":"on","timestamp":1777630000,"nonce":"00112233445566778899aabbccddeeff","signature":"wrong-signature","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, ModelMismatchCausesEarlyReturn) {
  std::string payload =
    R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":"wrong-model","featureUuid":"f1","featureName":"on","timestamp":)" +
    std::to_string(time(nullptr)) +
    R"(,"nonce":"00112233445566778899aabbccddeeff","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, NullRequiredFieldsSkipsEntryAndStillSends) {
  // signature is missing — the entry should be skipped (continue), and the
  // outer ir_send_signal() at the end of the loop should still fire.
  std::string payload =
    std::string(R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":")") + MODEL + R"(","featureUuid":"f1","featureName":"on","value":1}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, WrongDeviceUuidCausesEarlyReturn) {
  std::string payload =
      std::string(R"([{"deviceUuid":"wrong-device","mac":"aa:bb:cc:dd:ee:ff","model":")") +
      MODEL + R"(","featureUuid":"f1","featureName":"on","timestamp":)" +
      std::to_string(time(nullptr)) +
      R"(,"nonce":"00112233445566778899aabbccddeeff","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, WrongMacCausesEarlyReturn) {
  std::string payload =
      std::string(R"([{"deviceUuid":"device-uuid-test-0000-000000000000","mac":"11:22:33:44:55:66","model":")") +
      MODEL + R"(","featureUuid":"f1","featureName":"on","timestamp":)" +
      std::to_string(time(nullptr)) +
      R"(,"nonce":"00112233445566778899aabbccddeeff","signature":"aabbccddeeff00112233445566778899aabbccddeeff00112233445566778899","payload":{"value":1}}])";
  sendCommand(payload);

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, WrongFeatureUuidCausesEarlyReturn) {
  sendCommand(validPayload("on", "1"));

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

TEST_F(IrLgTest, FeatureUuidMismatchedToFeatureNameCausesEarlyReturn) {
  sendCommand("[" + validEntry("on", "1", "f2") + "]");

  EXPECT_FALSE(IrLgMockState::instance().on_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 0);
}

// ===========================================================================
// Feature: "on"
// ===========================================================================

TEST_F(IrLgTest, OnValueOneCallsAcOnThenSend) {
  sendCommand("[" + validEntry("on", "1", "f1") + "]");

  EXPECT_TRUE(IrLgMockState::instance().on_called);
  EXPECT_FALSE(IrLgMockState::instance().off_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, OnValueZeroCallsAcOffThenSendAndReturnsEarly) {
  // "on"=0 triggers the early-return path: off() + ir_send_signal() + return.
  // The outer ir_send_signal() must NOT fire a second time.
  sendCommand("[" + validEntry("on", "0", "f1") + "]");

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
  sendCommand("[" + validEntry("setpoint", "22", "f2") + "]");
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 22.0f);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, DuplicateCommandNonceIsRejected) {
  sendCommand("[" + validEntry("setpoint", "22", "f2") + "]");
  sendCommand("[" + validEntry("setpoint", "24", "f2") + "]");

  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 22.0f);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, SetpointAtMinBoundaryCallsSetTemp) {
  sendCommand("[" + validEntry("setpoint", "18", "f2") + "]");  // TEMP_MIN = 18
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 18.0f);
}

TEST_F(IrLgTest, SetpointAtMaxBoundaryCallsSetTemp) {
  sendCommand("[" + validEntry("setpoint", "30", "f2") + "]");  // TEMP_MAX = kLgAcMaxTemp = 30
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 30.0f);
}

TEST_F(IrLgTest, SetpointBelowMinIsSkipped) {
  sendCommand("[" + validEntry("setpoint", "17", "f2") + "]");
  EXPECT_FALSE(IrLgMockState::instance().settemp_called);
  // Outer ir_send_signal() still fires (entry was skipped via continue).
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, SetpointAboveMaxIsSkipped) {
  sendCommand("[" + validEntry("setpoint", "31", "f2") + "]");
  EXPECT_FALSE(IrLgMockState::instance().settemp_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "mode"
// ===========================================================================

TEST_F(IrLgTest, ModeOneSetsModeCool) {
  sendCommand("[" + validEntry("mode", "1", "f3") + "]");
  EXPECT_TRUE(IrLgMockState::instance().setmode_called);
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcCool));
}

TEST_F(IrLgTest, ModeTwoSetsModeAuto) {
  sendCommand("[" + validEntry("mode", "2", "f3") + "]");
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcAuto));
}

TEST_F(IrLgTest, ModeThreeSetsModeHeat) {
  sendCommand("[" + validEntry("mode", "3", "f3") + "]");
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcHeat));
}

TEST_F(IrLgTest, ModeFourSetsModeFan) {
  sendCommand("[" + validEntry("mode", "4", "f3") + "]");
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcFan));
}

TEST_F(IrLgTest, ModeFiveSetsModeDry) {
  sendCommand("[" + validEntry("mode", "5", "f3") + "]");
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcDry));
}

TEST_F(IrLgTest, ModeUnsupportedValueDoesNotCallSetMode) {
  sendCommand("[" + validEntry("mode", "99", "f3") + "]");
  EXPECT_FALSE(IrLgMockState::instance().setmode_called);
  // send still fires
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Feature: "fanSpeed"
// ===========================================================================

TEST_F(IrLgTest, FanSpeedOneSetsMinFan) {
  sendCommand("[" + validEntry("fanSpeed", "1", "f4") + "]");
  EXPECT_TRUE(IrLgMockState::instance().setfan_called);
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanLowest));
}

TEST_F(IrLgTest, FanSpeedTwoSetsMedFan) {
  sendCommand("[" + validEntry("fanSpeed", "2", "f4") + "]");
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanMedium));
}

TEST_F(IrLgTest, FanSpeedThreeSetsMaxFan) {
  sendCommand("[" + validEntry("fanSpeed", "3", "f4") + "]");
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanHigh));
}

TEST_F(IrLgTest, FanSpeedFourSetsAutoFan) {
  sendCommand("[" + validEntry("fanSpeed", "4", "f4") + "]");
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanAuto));
}

TEST_F(IrLgTest, FanSpeedFiveIsNotSupported) {
  // cmd=5 hits the "Auto0 not supported" branch — setFan must NOT be called.
  sendCommand("[" + validEntry("fanSpeed", "5", "f4") + "]");
  EXPECT_FALSE(IrLgMockState::instance().setfan_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

TEST_F(IrLgTest, FanSpeedUnsupportedValueDoesNotCallSetFan) {
  sendCommand("[" + validEntry("fanSpeed", "99", "f4") + "]");
  EXPECT_FALSE(IrLgMockState::instance().setfan_called);
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}

// ===========================================================================
// Multi-feature payload
// ===========================================================================

TEST_F(IrLgTest, MultipleFeaturesAllAppliedWithSingleSend) {
  // on=1, setpoint=24, mode=1(Cool), fanSpeed=2(Med) — all in one message.
  std::string payload = "[" +
    validEntry("on",       "1",  "f1", "00112233445566778899aabbccddeeff") + "," +
    validEntry("setpoint", "24", "f2", "10112233445566778899aabbccddeeff") + "," +
    validEntry("mode",     "1",  "f3", "20112233445566778899aabbccddeeff") + "," +
    validEntry("fanSpeed", "2",  "f4", "30112233445566778899aabbccddeeff") + "]";
  sendCommand(payload);

  EXPECT_TRUE(IrLgMockState::instance().on_called);
  EXPECT_TRUE(IrLgMockState::instance().settemp_called);
  EXPECT_FLOAT_EQ(IrLgMockState::instance().last_temp, 24.0f);
  EXPECT_EQ(IrLgMockState::instance().last_mode, static_cast<int>(kLgAcCool));
  EXPECT_EQ(IrLgMockState::instance().last_fan, static_cast<int>(kLgAcFanMedium));
  // Only one send() call — via the outer ir_send_signal() at the end.
  EXPECT_EQ(IrLgMockState::instance().send_count, 1);
}
