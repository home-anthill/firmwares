#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Preferences.h"

#include "secrets.h"   // MODEL, API_TOKEN, TEMP_MIN/MAX come from controller.cpp defines
#include "storage.h"
#include "controller.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build a single JSON object entry (no array brackets) with correct credentials.
static std::string validEntry(const char* featureName, const char* valueStr, const char* featureUuid = "f0") {
  std::string s = R"({"apiToken":")";
  s += API_TOKEN;
  s += R"(","deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":")";
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

static char   g_dummy_uuid[37] = "device-uuid-test-0000-000000000000";
static char   g_dummy_mac[18] = "aa:bb:cc:dd:ee:ff";
static JsonDocument g_dummy_doc;

static void sendConfig(const std::string& json) {
  g_dummy_doc.clear();
  JsonArray dummy_features = g_dummy_doc.to<JsonArray>();
  JsonObject setpoint_feature = dummy_features.add<JsonObject>();
  setpoint_feature["uuid"] = "f1";
  setpoint_feature["name"] = "setpoint";
  JsonObject tolerance_feature = dummy_features.add<JsonObject>();
  tolerance_feature["uuid"] = "f2";
  tolerance_feature["name"] = "tolerance";
  auto* p = reinterpret_cast<uint8_t*>(const_cast<char*>(json.c_str()));
  set_configuration(g_dummy_uuid, g_dummy_mac, dummy_features, p,
                    static_cast<unsigned int>(json.size()));
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Preferences::reset();
  }
};

// ===========================================================================
// get_setpoint — default value when nothing is stored
// ===========================================================================

TEST_F(ControllerTest, GetSetpointReturnsDefaultWhenNotStored) {
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);
}

// ===========================================================================
// get_tolerance — default value when nothing is stored
// ===========================================================================

TEST_F(ControllerTest, GetToleranceReturnsDefaultWhenNotStored) {
  EXPECT_FLOAT_EQ(get_tolerance(), 5.0f);
}

// ===========================================================================
// set_configuration — JSON parsing errors
// ===========================================================================

TEST_F(ControllerTest, MalformedJsonDoesNothing) {
  sendConfig("not-json{{{");

  // Defaults must be unchanged.
  EXPECT_FLOAT_EQ(get_setpoint(),  20.0f);
  EXPECT_FLOAT_EQ(get_tolerance(),  5.0f);
}

TEST_F(ControllerTest, EmptyArrayDoesNothing) {
  sendConfig("[]");

  EXPECT_FLOAT_EQ(get_setpoint(),  20.0f);
  EXPECT_FLOAT_EQ(get_tolerance(),  5.0f);
}

// ===========================================================================
// set_configuration — security validation
// ===========================================================================

TEST_F(ControllerTest, WrongModelIsRejected) {
  std::string payload =
    std::string(R"([{"apiToken":")") + API_TOKEN + R"(","deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":"wrong-model","featureUuid":"f1","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // unchanged
}

TEST_F(ControllerTest, WrongApiTokenIsRejected) {
  std::string payload =
    std::string(R"([{"apiToken":"wrong-token","deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","model":")") + MODEL + R"(","featureUuid":"f1","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // unchanged
}

TEST_F(ControllerTest, MissingRequiredFieldsIsRejected) {
  // model field absent
  std::string payload =
    std::string(R"([{"apiToken":")") + API_TOKEN + R"(","deviceUuid":"device-uuid-test-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","featureUuid":"f1","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // unchanged
}

TEST_F(ControllerTest, WrongDeviceUuidIsRejected) {
  std::string payload =
    std::string(R"([{"apiToken":")") + API_TOKEN + R"(","deviceUuid":"wrong-device","mac":"aa:bb:cc:dd:ee:ff","model":")" + MODEL + R"(","featureUuid":"f1","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);
}

TEST_F(ControllerTest, WrongMacIsRejected) {
  std::string payload =
    std::string(R"([{"apiToken":")") + API_TOKEN + R"(","deviceUuid":"device-uuid-test-0000-000000000000","mac":"11:22:33:44:55:66","model":")" + MODEL + R"(","featureUuid":"f1","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);
}

TEST_F(ControllerTest, WrongFeatureUuidIsRejected) {
  sendConfig(validPayload("setpoint", "22"));
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);
}

TEST_F(ControllerTest, FeatureUuidMismatchedToFeatureNameIsRejected) {
  sendConfig("[" + validEntry("setpoint", "22", "f2") + "]");
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);
}

// ===========================================================================
// set_configuration — setpoint validation (TEMP_MIN=10, TEMP_MAX=25)
// ===========================================================================

TEST_F(ControllerTest, SetpointInRangeIsStored) {
  sendConfig("[" + validEntry("setpoint", "22", "f1") + "]");
  EXPECT_FLOAT_EQ(get_setpoint(), 22.0f);
}

TEST_F(ControllerTest, SetpointAtMinBoundaryIsAccepted) {
  sendConfig("[" + validEntry("setpoint", "10", "f1") + "]");
  EXPECT_FLOAT_EQ(get_setpoint(), 10.0f);
}

TEST_F(ControllerTest, SetpointAtMaxBoundaryIsAccepted) {
  sendConfig("[" + validEntry("setpoint", "25", "f1") + "]");
  EXPECT_FLOAT_EQ(get_setpoint(), 25.0f);
}

TEST_F(ControllerTest, SetpointBelowMinIsRejected) {
  sendConfig("[" + validEntry("setpoint", "9", "f1") + "]");
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // default unchanged
}

TEST_F(ControllerTest, SetpointAboveMaxIsRejected) {
  sendConfig("[" + validEntry("setpoint", "26", "f1") + "]");
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // default unchanged
}

// ===========================================================================
// set_configuration — tolerance validation (TOLERANCE_MIN=0, TOLERANCE_MAX=20)
// ===========================================================================

TEST_F(ControllerTest, ToleranceInRangeIsStored) {
  sendConfig("[" + validEntry("tolerance", "3", "f2") + "]");
  EXPECT_FLOAT_EQ(get_tolerance(), 3.0f);
}

TEST_F(ControllerTest, ToleranceAtMinBoundaryIsAccepted) {
  sendConfig("[" + validEntry("tolerance", "0", "f2") + "]");
  EXPECT_FLOAT_EQ(get_tolerance(), 0.0f);
}

TEST_F(ControllerTest, ToleranceAtMaxBoundaryIsAccepted) {
  sendConfig("[" + validEntry("tolerance", "20", "f2") + "]");
  EXPECT_FLOAT_EQ(get_tolerance(), 20.0f);
}

TEST_F(ControllerTest, ToleranceAboveMaxIsRejected) {
  sendConfig("[" + validEntry("tolerance", "21", "f2") + "]");
  EXPECT_FLOAT_EQ(get_tolerance(), 5.0f);  // default unchanged
}

// ===========================================================================
// set_configuration — multi-entry payload: both features stored in one call
// ===========================================================================

TEST_F(ControllerTest, BothSetpointAndToleranceInSinglePayload) {
  std::string payload = "[" + validEntry("setpoint", "18", "f1") + "," + validEntry("tolerance", "2", "f2") + "]";
  sendConfig(payload);

  EXPECT_FLOAT_EQ(get_setpoint(),  18.0f);
  EXPECT_FLOAT_EQ(get_tolerance(),  2.0f);
}

TEST_F(ControllerTest, InvalidEntryInMultiPayloadRejectsAll) {
  // setpoint=22 (valid) then setpoint=9 (invalid) — the invalid entry causes
  // early return so storage_set_feature_values is never called.
  std::string payload = "[" + validEntry("setpoint", "22", "f1") + "," + validEntry("setpoint", "9", "f2") + "]";
  sendConfig(payload);

  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // default — nothing stored
}
