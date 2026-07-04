#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
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
#include "dht_sensor.h"
#include "light_sensor.h"
#include "feature_values.h"

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

// --- display ----------------------------------------------------------------

void init_display(uint8_t) {}
void update_display() {}
void display_force_update() {}
void display_show_message(const char*, const char*) {}
void display_set_connectivity_status(bool, bool) {}
void display_sleep_for(unsigned long) {}

// --- dht_sensor (controllable return values) --------------------------------

static float g_dht_temp = NAN;
static float g_dht_hum  = NAN;
void  dht_init_sensor()     {}
float dht_get_temperature() { return g_dht_temp; }
float dht_get_humidity()    { return g_dht_hum;  }

// --- light_sensor (controllable return value) --------------------------------

static long g_light_lux = 0;
void light_init_sensor() {}
long light_get_value()   { return g_light_lux; }

// --- feature_values ---------------------------------------------------------

struct FeatureValueSetCall {
  std::string name;
  float value;
};

struct FeatureValueCapture {
  std::vector<FeatureValueSetCall> set_calls;
  int init_count{0};

  static FeatureValueCapture& instance() {
    static FeatureValueCapture s;
    return s;
  }
  static void reset() { instance() = FeatureValueCapture{}; }
};

void feature_values_init(JsonArray /*features*/) {
  FeatureValueCapture::instance().init_count++;
}

void feature_values_clear() {
  FeatureValueCapture::reset();
}

size_t feature_values_count() {
  return 0;
}

bool feature_values_set(const char* name, float value) {
  FeatureValueCapture::instance().set_calls.push_back({
    name ? name : "",
    value
  });
  return true;
}

bool feature_values_get(size_t /*index*/, FeatureValue* /*value*/) {
  return false;
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
void         read_and_send_dht_value();
void         read_and_send_light_value();
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
    FeatureValueCapture::reset();
    g_dht_temp       = NAN;
    g_dht_hum        = NAN;
    g_light_lux      = 0;
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
  addFeature("temperature", "temp-uuid-0000-0000-000000000001");
  addFeature("humidity",    "hum-uuid-0000-0000-000000000002");

  char buf[37] = {};
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  EXPECT_STREQ(buf, "temp-uuid-0000-0000-000000000001");

  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "humidity"));
  EXPECT_STREQ(buf, "hum-uuid-0000-0000-000000000002");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseWhenNotFound) {
  addFeature("temperature", "temp-uuid-0000-0000-000000000001");

  char buf[37] = {};
  bool found = get_feature_uuid_by_name(buf, sizeof(buf), "light");
  EXPECT_FALSE(found);
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameReturnsFalseOnEmptyFeatures) {
  char buf[37] = {};
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  EXPECT_STREQ(buf, "");
}

TEST_F(MainInoTest, GetFeatureUuidByNameSkipsEntriesWithNullFields) {
  // Entry missing uuid — should be skipped.
  JsonObject bad = saved_features.add<JsonObject>();
  bad["name"] = "temperature";
  // no "uuid" field → getFeatureUuidByName treats it as null

  addFeature("humidity", "hum-uuid-0000-0000-000000000002");

  char buf[37] = {};
  // temperature has no uuid → skipped → not found
  EXPECT_FALSE(get_feature_uuid_by_name(buf, sizeof(buf), "temperature"));
  // humidity is fine
  EXPECT_TRUE(get_feature_uuid_by_name(buf, sizeof(buf), "humidity"));
}

TEST_F(MainInoTest, GetFeatureUuidByNameTruncatesUuidToBufferSize) {
  addFeature("temperature", "temp-uuid-0000-0000-000000000001");

  char buf[5] = {};
  get_feature_uuid_by_name(buf, sizeof(buf), "temperature");
  // Buffer of 5: 4 chars + null terminator.
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
  EXPECT_STREQ(arr[0]["name"].as<const char*>(), "temperature");
  EXPECT_STREQ(arr[1]["name"].as<const char*>(), "humidity");
  EXPECT_STREQ(arr[2]["name"].as<const char*>(), "light");
  EXPECT_STREQ(arr[3]["name"].as<const char*>(), "online");
}

TEST_F(MainInoTest, BuildFeaturesHasCorrectFieldsPerEntry) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  // temperature
  EXPECT_STREQ(arr[0]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[0]["unit"].as<const char*>(), "°C");
  EXPECT_TRUE(arr[0]["enable"].as<bool>());
  EXPECT_EQ(arr[0]["order"].as<int>(), 1);

  // humidity
  EXPECT_STREQ(arr[1]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[1]["unit"].as<const char*>(), "%");
  EXPECT_TRUE(arr[1]["enable"].as<bool>());
  EXPECT_EQ(arr[1]["order"].as<int>(), 2);

  // light
  EXPECT_STREQ(arr[2]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[2]["unit"].as<const char*>(), "lux");
  EXPECT_TRUE(arr[2]["enable"].as<bool>());
  EXPECT_EQ(arr[2]["order"].as<int>(), 3);

  // online
  EXPECT_STREQ(arr[3]["type"].as<const char*>(), "sensor");
  EXPECT_STREQ(arr[3]["unit"].as<const char*>(), "-");
  EXPECT_TRUE(arr[3]["enable"].as<bool>());
  EXPECT_EQ(arr[3]["order"].as<int>(), 4);
}

TEST_F(MainInoTest, BuildFeaturesIncludesAdmissionSpecs) {
  JsonDocument result = buildFeatures();
  JsonArray    arr    = result.as<JsonArray>();

  JsonObject temperatureSpec = arr[0]["spec"].as<JsonObject>();
  ASSERT_FALSE(temperatureSpec.isNull());
  EXPECT_STREQ(temperatureSpec["format"].as<const char*>(), "float");
  EXPECT_FLOAT_EQ(temperatureSpec["min"].as<float>(), -40.0f);
  EXPECT_FLOAT_EQ(temperatureSpec["max"].as<float>(), 80.0f);
  EXPECT_FLOAT_EQ(temperatureSpec["step"].as<float>(), 0.05f);
  EXPECT_TRUE(temperatureSpec["list"].isNull());

  JsonObject humiditySpec = arr[1]["spec"].as<JsonObject>();
  ASSERT_FALSE(humiditySpec.isNull());
  EXPECT_STREQ(humiditySpec["format"].as<const char*>(), "float");
  EXPECT_FLOAT_EQ(humiditySpec["min"].as<float>(), 0.0f);
  EXPECT_FLOAT_EQ(humiditySpec["max"].as<float>(), 100.0f);
  EXPECT_FLOAT_EQ(humiditySpec["step"].as<float>(), 2.5f);
  EXPECT_TRUE(humiditySpec["list"].isNull());

  JsonObject lightSpec = arr[2]["spec"].as<JsonObject>();
  ASSERT_FALSE(lightSpec.isNull());
  EXPECT_STREQ(lightSpec["format"].as<const char*>(), "int");
  EXPECT_FLOAT_EQ(lightSpec["min"].as<float>(), 0.0f);
  EXPECT_FLOAT_EQ(lightSpec["max"].as<float>(), 40000.0f);
  EXPECT_FLOAT_EQ(lightSpec["step"].as<float>(), 1.0f);
  EXPECT_TRUE(lightSpec["list"].isNull());

  JsonObject onlineSpec = arr[3]["spec"].as<JsonObject>();
  ASSERT_FALSE(onlineSpec.isNull());
  EXPECT_STREQ(onlineSpec["format"].as<const char*>(), "bool");
  EXPECT_TRUE(onlineSpec["min"].isNull());
  EXPECT_TRUE(onlineSpec["max"].isNull());
  EXPECT_TRUE(onlineSpec["step"].isNull());
  EXPECT_TRUE(onlineSpec["list"].isNull());
}

// =============================================================================
// send_online_status
// =============================================================================

TEST_F(MainInoTest, SendOnlineStatusPublishesWhenFeatureFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("online", "online-uuid-0000-0000-000000000004");

  send_online_status();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "online");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 1.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].device_uuid, "device-uuid-test-0000-000000000000");
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "online-uuid-0000-0000-000000000004");
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "online");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1.0f);
}

TEST_F(MainInoTest, SendOnlineStatusSkipsPublishWhenFeatureNotFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);

  send_online_status();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "online");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1.0f);
}

// =============================================================================
// read_and_send_dht_value
// =============================================================================

TEST_F(MainInoTest, ReadDhtPublishesBothReadingsWhenValid) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("temperature", "temp-uuid-0000-0000-000000000001");
  addFeature("humidity",    "hum-uuid-0000-0000-000000000002");
  g_dht_temp = 23.5f;
  g_dht_hum  = 60.0f;

  read_and_send_dht_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 2u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "temperature");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 23.5f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[1].type, "humidity");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[1].value, 60.0f);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 2u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "temperature");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 23.5f);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[1].name, "humidity");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[1].value, 60.0f);
}

TEST_F(MainInoTest, ReadDhtSkipsTemperatureWhenNan) {
  addFeature("temperature", "temp-uuid-0000-0000-000000000001");
  addFeature("humidity",    "hum-uuid-0000-0000-000000000002");
  g_dht_temp = NAN;   // invalid reading
  g_dht_hum  = 60.0f;

  read_and_send_dht_value();

  // Only the humidity publish should occur.
  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "humidity");
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "humidity");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 60.0f);
}

TEST_F(MainInoTest, ReadDhtSkipsHumidityWhenNan) {
  addFeature("temperature", "temp-uuid-0000-0000-000000000001");
  addFeature("humidity",    "hum-uuid-0000-0000-000000000002");
  g_dht_temp = 23.5f;
  g_dht_hum  = NAN;   // invalid reading

  read_and_send_dht_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "temperature");
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "temperature");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 23.5f);
}

TEST_F(MainInoTest, ReadDhtSkipsPublishWhenFeatureNotFound) {
  // No features registered → UUID lookup fails → nothing published.
  g_dht_temp = 23.5f;
  g_dht_hum  = 60.0f;

  read_and_send_dht_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 2u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "temperature");
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[1].name, "humidity");
}

// =============================================================================
// read_and_send_light_value
// =============================================================================

TEST_F(MainInoTest, ReadLightPublishesValueWhenFeatureFound) {
  strncpy(saved_device_uuid, "device-uuid-test-0000-000000000000", 36);
  addFeature("light", "light-uuid-0000-0000-000000000003");
  g_light_lux = 1500;

  read_and_send_light_value();

  ASSERT_EQ(MqttNotifyCapture::instance().calls.size(), 1u);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].type, "light");
  EXPECT_FLOAT_EQ(MqttNotifyCapture::instance().calls[0].value, 1500.0f);
  EXPECT_EQ(MqttNotifyCapture::instance().calls[0].feature_uuid, "light-uuid-0000-0000-000000000003");
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "light");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1500.0f);
}

TEST_F(MainInoTest, ReadLightSkipsPublishWhenFeatureNotFound) {
  // No features registered → UUID lookup fails → nothing published.
  g_light_lux = 1500;

  read_and_send_light_value();

  EXPECT_EQ(MqttNotifyCapture::instance().calls.size(), 0u);
  ASSERT_EQ(FeatureValueCapture::instance().set_calls.size(), 1u);
  EXPECT_EQ(FeatureValueCapture::instance().set_calls[0].name, "light");
  EXPECT_FLOAT_EQ(FeatureValueCapture::instance().set_calls[0].value, 1500.0f);
}
