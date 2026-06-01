#include <gtest/gtest.h>
#include <cstring>
#include <functional>
#include <vector>
#include <string>

// Mock headers — must come before any production include.
#include <Arduino.h>
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "PubSubClient.h"

#include <ArduinoJson.h>

// secrets.h must come before any firmware header that uses SSL/MANUFACTURER/MODEL.
#include "secrets.h"

// Headers from the firmware (needed for function signatures only).
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "registration.h"
#include "storage.h"
#include "ir_beko.h"

// =============================================================================
// Stubs — provide every external symbol that <main-project-arduino-file>.ino references.
//
// None of the dependent .cpp files are compiled into this target.  These stubs
// link cleanly AND let tests control the observable behaviour of each boundary.
// setup() and loop() compile and link but are never called from tests: they
// orchestrate full I/O sequences that are not meaningfully unit-testable.
// =============================================================================

// --- Globals defined in wifi_handler.cpp and mqtt_handler.cpp ---------------

#if SSL == true
WiFiClientSecure wifi_client;
#else
WiFiClient wifi_client;
#endif
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

// --- ir_beko — capture ir_send_command calls ----------------------------------

struct IrSendCommandCall {
  std::string topic;
  std::string payload;
  unsigned int length;
};

struct IrCommandCapture {
  std::vector<IrSendCommandCall> calls;

  static IrCommandCapture& instance() {
    static IrCommandCapture s;
    return s;
  }
  static void reset() { instance().calls.clear(); }
};

void ir_init() {}

void ir_send_command(const char* /*uuid*/, const char* /*mac*/,
                     JsonArray /*features*/, char* topic, uint8_t* payload,
                     unsigned int length) {
  IrCommandCapture::instance().calls.push_back({
    topic   ? topic   : "",
    payload ? std::string(reinterpret_cast<char*>(payload), length) : "",
    length
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
// =============================================================================

bool         get_feature_uuid_by_name(char* featureUuid, size_t max_len, const char* name);
JsonDocument buildFeatures();

// Expose .ino globals so the fixture can reset them between tests.
extern JsonDocument doc_features;
extern JsonArray    saved_features;
extern char         saved_device_uuid[37];

// =============================================================================
// Fixture
// =============================================================================

class MainInoTest : public ::testing::Test {
protected:
  void SetUp() override {
    doc_features.clear();
    saved_features = doc_features.to<JsonArray>();
    memset(saved_device_uuid, 0, sizeof(saved_device_uuid));

    IrCommandCapture::reset();
    g_stored_uuid_len = 0;
    memset(g_stored_uuid, 0, sizeof(g_stored_uuid));
  }

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
  addFeature("on",       "on-uuid-0000-0000-000000000001");
  addFeature("setpoint", "sp-uuid-0000-0000-000000000002");
  addFeature("mode",     "mo-uuid-0000-0000-000000000003");
  addFeature("fanSpeed", "fn-uuid-0000-0000-000000000004");

  char buf[37] = {};
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "on"));
  EXPECT_STREQ(buf, "on-uuid-0000-0000-000000000001");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "setpoint"));
  EXPECT_STREQ(buf, "sp-uuid-0000-0000-000000000002");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "mode"));
  EXPECT_STREQ(buf, "mo-uuid-0000-0000-000000000003");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "fanSpeed"));
  EXPECT_STREQ(buf, "fn-uuid-0000-0000-000000000004");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseWhenNotFound) {
  addFeature("on", "on-uuid-0000-0000-000000000001");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseOnEmptyFeatures) {
  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "on"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameSkipsEntriesWithNullFields) {
  JsonObject bad = saved_features.add<JsonObject>();
  bad["name"] = "on";
  // no "uuid" field → skipped

  addFeature("setpoint", "sp-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "on"));
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "setpoint"));
}

TEST_F(MainInoTest, GetFeatureUuidByNameTruncatesUuidToBufferSize) {
  addFeature("on", "on-uuid-0000-0000-000000000001");

  char buf[5] = {};
  get_feature_uuid_by_name(buf, sizeof(buf), "on");
  EXPECT_EQ(buf[sizeof(buf) - 1], '\0');
  EXPECT_EQ(strnlen(buf, sizeof(buf)), 4u);
}

// =============================================================================
// buildFeatures
// =============================================================================

TEST_F(MainInoTest, BuildFeaturesReturnsFourFeatures) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  ASSERT_EQ(arr.size(), 4u);
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "on");
  EXPECT_STREQ(arr[1]["name"].as<const char*>(), "setpoint");
  EXPECT_STREQ(arr[2]["name"].as<const char*>(), "mode");
  EXPECT_STREQ(arr[3]["name"].as<const char*>(), "fanSpeed");
}

TEST_F(MainInoTest, BuildFeaturesHasCorrectFieldsPerEntry) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  // All features are type "controller"
  for (size_t i = 0; i < arr.size(); i++) {
    EXPECT_STREQ(arr[i]["type"].as<const char*>(), "controller") << "entry " << i;
    EXPECT_TRUE(arr[i]["enable"].as<bool>()) << "entry " << i;
  }

  // Per-feature specifics
  EXPECT_STREQ(arr[0]["unit"].as<const char*>(), "-");
  EXPECT_EQ(arr[0]["order"].as<int>(), 1);

  EXPECT_STREQ(arr[1]["unit"].as<const char*>(), "°C");
  EXPECT_EQ(arr[1]["order"].as<int>(), 2);

  EXPECT_STREQ(arr[2]["unit"].as<const char*>(), "-");
  EXPECT_EQ(arr[2]["order"].as<int>(), 3);

  EXPECT_STREQ(arr[3]["unit"].as<const char*>(), "-");
  EXPECT_EQ(arr[3]["order"].as<int>(), 4);
}

TEST_F(MainInoTest, BuildFeaturesIncludesAdmissionSpecs) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  JsonObject onSpec = arr[0]["spec"].as<JsonObject>();
  ASSERT_FALSE(onSpec.isNull());
  EXPECT_STREQ(onSpec["format"].as<const char*>(), "bool");
  EXPECT_TRUE(onSpec["min"].isNull());
  EXPECT_TRUE(onSpec["max"].isNull());
  EXPECT_TRUE(onSpec["step"].isNull());
  EXPECT_TRUE(onSpec["list"].isNull());

  JsonObject setpointSpec = arr[1]["spec"].as<JsonObject>();
  ASSERT_FALSE(setpointSpec.isNull());
  EXPECT_STREQ(setpointSpec["format"].as<const char*>(), "int");
  EXPECT_FLOAT_EQ(setpointSpec["min"].as<float>(), 17.0f);
  EXPECT_FLOAT_EQ(setpointSpec["max"].as<float>(), 30.0f);
  EXPECT_FLOAT_EQ(setpointSpec["step"].as<float>(), 1.0f);
  EXPECT_TRUE(setpointSpec["list"].isNull());

  JsonObject modeSpec = arr[2]["spec"].as<JsonObject>();
  ASSERT_FALSE(modeSpec.isNull());
  EXPECT_STREQ(modeSpec["format"].as<const char*>(), "list");
  EXPECT_TRUE(modeSpec["min"].isNull());
  EXPECT_TRUE(modeSpec["max"].isNull());
  EXPECT_TRUE(modeSpec["step"].isNull());
  JsonArray modeList = modeSpec["list"].as<JsonArray>();
  ASSERT_EQ(modeList.size(), 5u);
  EXPECT_EQ(modeList[0]["value"].as<int>(), 0);
  EXPECT_STREQ(modeList[0]["text"].as<const char*>(), "Cool");
  EXPECT_EQ(modeList[1]["value"].as<int>(), 1);
  EXPECT_STREQ(modeList[1]["text"].as<const char*>(), "Dry");
  EXPECT_EQ(modeList[2]["value"].as<int>(), 2);
  EXPECT_STREQ(modeList[2]["text"].as<const char*>(), "Auto");
  EXPECT_EQ(modeList[3]["value"].as<int>(), 3);
  EXPECT_STREQ(modeList[3]["text"].as<const char*>(), "Heat");
  EXPECT_EQ(modeList[4]["value"].as<int>(), 4);
  EXPECT_STREQ(modeList[4]["text"].as<const char*>(), "Fan");

  JsonObject fanSpeedSpec = arr[3]["spec"].as<JsonObject>();
  ASSERT_FALSE(fanSpeedSpec.isNull());
  EXPECT_STREQ(fanSpeedSpec["format"].as<const char*>(), "list");
  EXPECT_TRUE(fanSpeedSpec["min"].isNull());
  EXPECT_TRUE(fanSpeedSpec["max"].isNull());
  EXPECT_TRUE(fanSpeedSpec["step"].isNull());
  JsonArray fanSpeedList = fanSpeedSpec["list"].as<JsonArray>();
  ASSERT_EQ(fanSpeedList.size(), 5u);
  EXPECT_EQ(fanSpeedList[0]["value"].as<int>(), 0);
  EXPECT_STREQ(fanSpeedList[0]["text"].as<const char*>(), "Auto0");
  EXPECT_EQ(fanSpeedList[1]["value"].as<int>(), 1);
  EXPECT_STREQ(fanSpeedList[1]["text"].as<const char*>(), "Max");
  EXPECT_EQ(fanSpeedList[2]["value"].as<int>(), 2);
  EXPECT_STREQ(fanSpeedList[2]["text"].as<const char*>(), "Med");
  EXPECT_EQ(fanSpeedList[3]["value"].as<int>(), 4);
  EXPECT_STREQ(fanSpeedList[3]["text"].as<const char*>(), "Min");
  EXPECT_EQ(fanSpeedList[4]["value"].as<int>(), 5);
  EXPECT_STREQ(fanSpeedList[4]["text"].as<const char*>(), "Auto");
}

// =============================================================================
// mqtt_callback — delegates to ir_send_command
// =============================================================================

// Forward-declare mqtt_callback (defined in ac-beko.ino, no header).
void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);

TEST_F(MainInoTest, MqttCallbackDelegatesToIrSendCommand) {
  const char* topic   = "devices/dev-uuid/values";
  const char* payload = R"([{"featureName":"on","value":1}])";
  auto* p = reinterpret_cast<uint8_t*>(const_cast<char*>(payload));
  unsigned int len = static_cast<unsigned int>(strlen(payload));

  mqtt_callback(const_cast<char*>(topic), p, len);

  ASSERT_EQ(IrCommandCapture::instance().calls.size(), 1u);
  EXPECT_EQ(IrCommandCapture::instance().calls[0].topic, topic);
  EXPECT_EQ(IrCommandCapture::instance().calls[0].length, len);
}
