#include <gtest/gtest.h>
#include <cstring>
#include <functional>
#include <vector>

// Mock headers — must come before any production include.
#include <Arduino.h>
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "PubSubClient.h"
#include "TimeAlarms.h"

#include <ArduinoJson.h>

// secrets.h must come before any firmware header that uses SSL/MANUFACTURER/MODEL.
#include "secrets.h"

// Headers from the firmware (needed for function signatures only).
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"

// =============================================================================
// Stubs — provide every external symbol that <main-project-arduino-file>.ino references.
//
// None of the dependent .cpp files are compiled into this target.  These stubs
// link cleanly AND let tests control the observable behaviour of each boundary.
// setup() and loop() compile and link but are never called from tests: they
// orchestrate full I/O sequences that are not meaningfully unit-testable.
// =============================================================================

// --- Globals defined in wifi_handler.cpp and mqtt_handler.cpp ---------------

WiFiClientSecure wifi_client;
PubSubClient     mqtt_client;

// --- wifi_handler -----------------------------------------------------------

void wifi_init_ca()             {}
void wifi_reconnect(char* /*mac*/) {}
int  wifi_get_status()          { return WL_CONNECTED; }

void wifi_connect(char* mac) {
  strncpy(mac, "aa:bb:cc:dd:ee:ff", 17);
  mac[17] = '\0';
}

// --- registration -----------------------------------------------------------

int register_secure_server(WiFiClientSecure& /*c*/, const char* /*mac*/,
                            const JsonDocument& /*f*/) { return 0; }
int register_insecure_server(WiFiClient& /*c*/, const char* /*mac*/,
                             const JsonDocument& /*f*/) { return 0; }

// --- mqtt_handler -----------------------------------------------------------

void mqtt_init(Client& /*c*/,
               std::function<void(char*, uint8_t*, unsigned int)> /*cb*/) {}
void mqtt_connect(const char* /*uuid*/) {}

// Capture every mqtt_notify_value() call so tests can inspect it.
struct NotifyCall {
  std::string device_uuid;
  std::string feature_uuid;
  std::string type;
  float       value;
};

struct MqttNotifyCapture {
  std::vector<NotifyCall> calls;

  static MqttNotifyCapture& instance() {
    static MqttNotifyCapture s;
    return s;
  }
  static void reset() { instance().calls.clear(); }
};

void mqtt_notify_value(const char* device_uuid, const char* feature_uuid,
                       const char* type, float value) {
  MqttNotifyCapture::instance().calls.push_back({
    device_uuid  ? device_uuid  : "",
    feature_uuid ? feature_uuid : "",
    type         ? type         : "",
    value
  });
}

// --- storage ----------------------------------------------------------------

void   storage_get_features(JsonArray /*arr*/) {}
size_t storage_set_features(JsonArray /*arr*/) { return 0; }

static char   g_stored_uuid[37] = {};
static size_t g_stored_uuid_len  = 0;

size_t storage_get_uuid(char* buf) {
  if (buf) { strncpy(buf, g_stored_uuid, 36); buf[36] = '\0'; }
  return g_stored_uuid_len;
}
size_t storage_set_uuid(const char* uuid) {
  strncpy(g_stored_uuid, uuid, 36);
  g_stored_uuid[36] = '\0';
  g_stored_uuid_len = strlen(g_stored_uuid);
  return g_stored_uuid_len;
}

// =============================================================================
// Forward-declare the private functions defined in <main-project-arduino-file>.ino.
// They have no header — declarations here match the definitions in the .ino.
// =============================================================================

bool         get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
JsonDocument buildFeatures();
void         send_online_status();

// Expose <main-project-arduino-file>.ino globals so the fixture can reset them between tests.
extern JsonDocument doc_features;
extern JsonArray    saved_features;
extern char         saved_device_uuid[37];

// =============================================================================
// Fixture — resets all shared state before every test.
// =============================================================================

class MainInoTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Clear the feature array stored in the .ino globals.
    doc_features.clear();
    saved_features = doc_features.to<JsonArray>();
    memset(saved_device_uuid, 0, sizeof(saved_device_uuid));

    // Reset stubs.
    MqttNotifyCapture::reset();
    g_stored_uuid_len = 0;
    memset(g_stored_uuid, 0, sizeof(g_stored_uuid));
  }

  // Helper: add a feature entry to saved_features.
  void addFeature(const char* name, const char* uuid) {
    JsonObject f = saved_features.add<JsonObject>();
    f["name"] = name;
    f["uuid"] = uuid;
  }
};

// =============================================================================
// get_feature_uuid_by_name
// =============================================================================

TEST_F(MainInoTest, GetFeatureUuidByNameFindsMatchingFeature) {
  addFeature("online", "online-uuid-0000-0000-000000000001");

  char buf[37] = {};
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "online"));
  EXPECT_STREQ(buf, "online-uuid-0000-0000-000000000001");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseWhenNotFound) {
  addFeature("online", "online-uuid-0000-0000-000000000001");

  char buf[37] = {};
  bool found = get_feature_uuid_by_name(buf, sizeof(buf), "temperature");
  EXPECT_FALSE(found);
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseOnEmptyFeatures) {
  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "online"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameSkipsEntriesWithNullFields) {
  // Entry missing uuid — should be skipped.
  JsonObject bad = saved_features.add<JsonObject>();
  bad["name"] = "online";
  // no "uuid" field → get_feature_uuid_by_name treats it as null

  addFeature("other", "other-uuid-0000-0000-000000000002");

  char buf[37] = {};
  // online has no uuid → skipped → not found
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "online"));
  // other is fine
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "other"));
}

TEST_F(MainInoTest, GetFeatureUuidByNameTruncatesUuidToBufferSize) {
  addFeature("online", "online-uuid-0000-0000-000000000001");

  char buf[5] = {};
  get_feature_uuid_by_name(buf, sizeof(buf), "online");
  // Buffer of 5: 4 chars + null terminator.
  EXPECT_EQ(buf[sizeof(buf) - 1], '\0');
  EXPECT_EQ(strnlen(buf, sizeof(buf)), 4u);
}

// =============================================================================
// buildFeatures
// =============================================================================

TEST_F(MainInoTest, BuildFeaturesReturnsOneFeature) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  ASSERT_EQ(arr.size(), 1u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "online");
}

TEST_F(MainInoTest, BuildFeaturesHasCorrectFieldsPerEntry) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  // online
  EXPECT_STREQ(arr[0]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[0]["unit"].as<const char*>(), "-");
  EXPECT_TRUE(arr[0]["enable"].as<bool>());
  EXPECT_EQ(arr[0]["order"].as<int>(), 1);
}

// =============================================================================
// send_online_status
// =============================================================================

TEST_F(MainInoTest, SendOnlineStatusPublishesWhenFeatureFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("online", "online-uuid-0000-0000-000000000001");

  send_online_status();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "online");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 1.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].device_uuid, "device-uuid-test-0000-000000000000");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "online-uuid-0000-0000-000000000001");
}

TEST_F(MainInoTest, SendOnlineStatusSkipsPublishWhenFeatureNotFound) {
  // No features registered → UUID lookup fails → nothing published.
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);

  send_online_status();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
}
