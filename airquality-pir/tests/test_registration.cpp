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
// and MODEL ("airquality-pir") as defined in secrets.h.
static std::string valid_response() {
  return R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","manufacturer":"ks89","model":"airquality-pir","features":[{"uuid":"feat-uuid-0000-0000-000000000001","type":"sensor","name":"airquality","enable":true,"order":1,"unit":"-"},{"uuid":"feat-uuid-0000-0000-000000000002","type":"sensor","name":"motion","enable":true,"order":2,"unit":"-"}]})";
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
  ASSERT_EQ(arr.size(), 2u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "airquality");
  EXPECT_STREQ(arr[1]["name"].as<const char*>(), "motion");

  // URL and payload are well-formed
  EXPECT_NE(HttpMockConfig::instance().last_url.find(SERVER_DOMAIN), std::string::npos);
  EXPECT_NE(HttpMockConfig::instance().last_payload.find(kMac), std::string::npos);
}

// ===========================================================================
// HTTP error status → return 1
// ===========================================================================

TEST_F(RegistrationTest, ReturnsOneOnBadHttpStatus) {
  HttpMockConfig::instance().response_code = 500;
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 1);

  HttpMockConfig::instance().response_code = 401;
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 1);
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
    R"({"mac":"aa:bb:cc:dd:ee:ff","manufacturer":"ks89","model":"airquality-pir","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 3);

  // mac absent
  HttpMockConfig::reset();
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","manufacturer":"ks89","model":"airquality-pir","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 3);
}

// ===========================================================================
// HTTP 200 OK but response/request mismatch → return 4
// ===========================================================================

TEST_F(RegistrationTest, ReturnsFourOnResponseMismatch) {
  HttpMockConfig::instance().response_code = HTTP_CODE_OK;

  // mac mismatch
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"ff:ff:ff:ff:ff:ff","manufacturer":"ks89","model":"airquality-pir","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 4);

  // manufacturer mismatch
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","manufacturer":"wrong","model":"airquality-pir","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 4);

  // model mismatch
  HttpMockConfig::instance().response_body =
    R"({"uuid":"test-uuid-0000-0000-000000000000","mac":"aa:bb:cc:dd:ee:ff","manufacturer":"ks89","model":"wrong-model","features":[]})";
  EXPECT_EQ(register_insecure_server(wifi_client, kMac, features_doc), 4);
}
