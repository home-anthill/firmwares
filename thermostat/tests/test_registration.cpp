#include <gtest/gtest.h>
#include <cstring>

// Mock headers must precede production source includes so that all
// #include <Arduino.h>, <WiFi.h>, etc. resolve to our stubs.
#include "Preferences.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"

#include <ArduinoJson.h>
#include "storage.h"
#include "registration.h"
#include "secrets.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr const char* kMac  = "aa:bb:cc:dd:ee:ff";
static constexpr const char* kUuid = "test-uuid-0000-0000-000000000000";

// A well-formed 200 OK response that matches kMac, MANUFACTURER ("ks89"),
// and MODEL ("thermostat") as defined in secrets.h.
static std::string valid_response() {
  return R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","manufacturer":"ks89","model":"thermostat","features":[{"uuid":"feat-uuid-0000-0000-000000000001","type":"controller","name":"setpoint","enable":true,"order":1,"unit":"°C"}]})";
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class RegistrationTest : public ::testing::Test {
protected:
  WiFiClient   wifi_client;
  JsonDocument features_doc;

  void SetUp() override {
    Preferences::reset();
    HttpMockConfig::reset();
    features_doc.to<JsonArray>();
  }
};

// ===========================================================================
// HTTP 409 Conflict — already registered, not an error
// ===========================================================================

TEST_F(RegistrationTest, ConflictReturnsZeroWithoutStorageWrite) {
  HttpMockConfig::instance().response_code = HTTP_CODE_CONFLICT;
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 0);

  char buf[37] = {};
  EXPECT_EQ(storage_get_uuid(buf), 0u);  // nothing written to storage
}

// ===========================================================================
// HTTP 200 OK — first registration
// ===========================================================================

TEST_F(RegistrationTest, OkResponseRegistersAndStoresData) {
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;
  HttpMockConfig::instance().response_body = valid_response();

  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 0);

  // UUID saved to storage
  char buf[37] = {};
  storage_get_uuid(buf);
  EXPECT_STREQ(buf, kUuid);

  // Features saved to storage
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  storage_get_features(arr);
  ASSERT_EQ(arr.size(), 1u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "setpoint");

  // URL contains the registration path (SERVER_PATH is identical in both real and
  // mock secrets.h, so it is a safe cross-file constant to assert against).
  EXPECT_NE(HttpMockConfig::instance().last_url.find(SERVER_PATH), std::string::npos);
  // Payload contains the MAC address
  EXPECT_NE(HttpMockConfig::instance().last_payload.find(kMac), std::string::npos);
}

// ===========================================================================
// HTTP 200 OK but malformed / incomplete JSON → return 3
// ===========================================================================

TEST_F(RegistrationTest, ReturnsThreeOnMalformedJson) {
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;
  HttpMockConfig::instance().response_body = "not-json{{{";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 3);
}

TEST_F(RegistrationTest, ReturnsThreeWhenRequiredFieldsMissingFromResponse) {
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;

  // uuid absent
  HttpMockConfig::instance().response_body =
    R"({"mac":"aa:bb:cc:dd:ee:ff","manufacturer":"ks89","model":"thermostat","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 3);

  // mac absent
  HttpMockConfig::reset();
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","manufacturer":"ks89","model":"thermostat","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 3);
}

// ===========================================================================
// HTTP 200 OK but response/request mismatch → return 4
// ===========================================================================

TEST_F(RegistrationTest, ReturnsFourOnResponseMismatch) {
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;

  // mac mismatch
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"ff:ff:ff:ff:ff:ff","manufacturer":"ks89","model":"thermostat","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 4);

  // manufacturer mismatch
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","manufacturer":"wrong","model":"thermostat","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 4);

  // model mismatch
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","manufacturer":"ks89","model":"wrong-model","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 4);
}

// ===========================================================================
// register_insecure_server_once — single-attempt variant
// ===========================================================================

TEST_F(RegistrationTest, OnceConflictReturnsZero) {
  HttpMockConfig::instance().response_code = HTTP_CODE_CONFLICT;
  EXPECT_EQ(register_insecure_server_once(wifi_client, kMac, features_doc), 0);
}

TEST_F(RegistrationTest, OnceOkResponseRegistersAndStoresData) {
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;
  HttpMockConfig::instance().response_body = valid_response();

  EXPECT_EQ(register_insecure_server_once(wifi_client, kMac, features_doc), 0);

  char buf[37] = {};
  storage_get_uuid(buf);
  EXPECT_STREQ(buf, kUuid);
}

TEST_F(RegistrationTest, OnceReturnsOneOnBadHttpStatus) {
  HttpMockConfig::instance().response_code = 500;
  EXPECT_EQ(register_insecure_server_once(wifi_client, kMac, features_doc), 1);
}

TEST_F(RegistrationTest, OnceReturnsFourOnResponseMismatch) {
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","manufacturer":"ks89","model":"wrong-model","features":[]})";
  EXPECT_EQ(register_insecure_server_once(wifi_client, kMac, features_doc), 4);
}
