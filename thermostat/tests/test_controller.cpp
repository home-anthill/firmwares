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

// Build a single-entry JSON array command payload with valid token/model.
static std::string validEntry(const char* featureName, float value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%g", static_cast<double>(value));
  std::string s = R"([{"apiToken":")";
  s += API_TOKEN;
  s += R"(","deviceUuid":"d","mac":"m","model":")";
  s += MODEL;
  s += R"(","featureUuid":"f","featureName":")";
  s += featureName;
  s += R"(","value":)";
  s += buf;
  s += "}]";
  return s;
}

static char   g_dummy_uuid[37] = "device-uuid-test-0000-000000000000";
static JsonDocument g_dummy_doc;

static void sendConfig(const std::string& json) {
  // Rebuild a fresh empty features array each call.
  g_dummy_doc.clear();
  JsonArray dummy_features = g_dummy_doc.to<JsonArray>();
  auto* p = reinterpret_cast<uint8_t*>(const_cast<char*>(json.c_str()));
  set_configuration(g_dummy_uuid, dummy_features, p,
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
    R"([{"apiToken":"473a4861-632b-4915-b01e-cf1d418966c6","deviceUuid":"d","mac":"m","model":"wrong-model","featureUuid":"f","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // unchanged
}

TEST_F(ControllerTest, WrongApiTokenIsRejected) {
  std::string payload =
    R"([{"apiToken":"wrong-token","deviceUuid":"d","mac":"m","model":"thermostat","featureUuid":"f","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // unchanged
}

TEST_F(ControllerTest, MissingRequiredFieldsIsRejected) {
  // model field absent
  std::string payload =
    R"([{"apiToken":"473a4861-632b-4915-b01e-cf1d418966c6","deviceUuid":"d","mac":"m","featureUuid":"f","featureName":"setpoint","value":22}])";
  sendConfig(payload);
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // unchanged
}

// ===========================================================================
// set_configuration — setpoint validation (TEMP_MIN=10, TEMP_MAX=25)
// ===========================================================================

TEST_F(ControllerTest, SetpointInRangeIsStored) {
  sendConfig(validEntry("setpoint", 22.0f));
  EXPECT_FLOAT_EQ(get_setpoint(), 22.0f);
}

TEST_F(ControllerTest, SetpointAtMinBoundaryIsAccepted) {
  sendConfig(validEntry("setpoint", 10.0f));
  EXPECT_FLOAT_EQ(get_setpoint(), 10.0f);
}

TEST_F(ControllerTest, SetpointAtMaxBoundaryIsAccepted) {
  sendConfig(validEntry("setpoint", 25.0f));
  EXPECT_FLOAT_EQ(get_setpoint(), 25.0f);
}

TEST_F(ControllerTest, SetpointBelowMinIsRejected) {
  sendConfig(validEntry("setpoint", 9.0f));
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // default unchanged
}

TEST_F(ControllerTest, SetpointAboveMaxIsRejected) {
  sendConfig(validEntry("setpoint", 26.0f));
  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // default unchanged
}

// ===========================================================================
// set_configuration — tolerance validation (TOLERANCE_MIN=0, TOLERANCE_MAX=20)
// ===========================================================================

TEST_F(ControllerTest, ToleranceInRangeIsStored) {
  sendConfig(validEntry("tolerance", 3.0f));
  EXPECT_FLOAT_EQ(get_tolerance(), 3.0f);
}

TEST_F(ControllerTest, ToleranceAtMinBoundaryIsAccepted) {
  sendConfig(validEntry("tolerance", 0.0f));
  EXPECT_FLOAT_EQ(get_tolerance(), 0.0f);
}

TEST_F(ControllerTest, ToleranceAtMaxBoundaryIsAccepted) {
  sendConfig(validEntry("tolerance", 20.0f));
  EXPECT_FLOAT_EQ(get_tolerance(), 20.0f);
}

TEST_F(ControllerTest, ToleranceAboveMaxIsRejected) {
  sendConfig(validEntry("tolerance", 21.0f));
  EXPECT_FLOAT_EQ(get_tolerance(), 5.0f);  // default unchanged
}

// ===========================================================================
// set_configuration — multi-entry payload: both features stored in one call
// ===========================================================================

TEST_F(ControllerTest, BothSetpointAndToleranceInSinglePayload) {
  std::string payload =
    R"([)"
    R"({"apiToken":"473a4861-632b-4915-b01e-cf1d418966c6","deviceUuid":"d","mac":"m","model":"thermostat","featureUuid":"f1","featureName":"setpoint","value":18},)"
    R"({"apiToken":"473a4861-632b-4915-b01e-cf1d418966c6","deviceUuid":"d","mac":"m","model":"thermostat","featureUuid":"f2","featureName":"tolerance","value":2})"
    R"(])";
  sendConfig(payload);

  EXPECT_FLOAT_EQ(get_setpoint(),  18.0f);
  EXPECT_FLOAT_EQ(get_tolerance(),  2.0f);
}

TEST_F(ControllerTest, InvalidEntryInMultiPayloadRejectsAll) {
  // setpoint=22 (valid) then setpoint=9 (invalid) — the invalid entry causes
  // early return so storage_set_feature_values is never called.
  std::string payload =
    R"([)"
    R"({"apiToken":"473a4861-632b-4915-b01e-cf1d418966c6","deviceUuid":"d","mac":"m","model":"thermostat","featureUuid":"f1","featureName":"setpoint","value":22},)"
    R"({"apiToken":"473a4861-632b-4915-b01e-cf1d418966c6","deviceUuid":"d","mac":"m","model":"thermostat","featureUuid":"f2","featureName":"setpoint","value":9})"
    R"(])";
  sendConfig(payload);

  EXPECT_FLOAT_EQ(get_setpoint(), 20.0f);  // default — nothing stored
}
