#include <gtest/gtest.h>
#include <cstring>

#include <ArduinoJson.h>
#include "Preferences.h"  // mock — must appear before storage.h pulls it in via Arduino.h path
#include "storage.h"

// ---------------------------------------------------------------------------
// Each test starts with a clean NVS store.
// ---------------------------------------------------------------------------

class StorageTest : public ::testing::Test {
protected:
  void SetUp() override {
    Preferences::reset();
  }
};

// ===========================================================================
// storage_set_uuid / storage_get_uuid
// ===========================================================================

TEST_F(StorageTest, SetAndGetUuid) {
  const char* uuid = "550e8400-e29b-41d4-a716-446655440000";

  size_t written = storage_set_uuid(uuid);
  EXPECT_EQ(written, strlen(uuid));

  char buf[37] = {};
  size_t read = storage_get_uuid(buf);
  EXPECT_EQ(read, strlen(uuid));
  EXPECT_STREQ(buf, uuid);
}

TEST_F(StorageTest, GetUuidReturnsZeroWhenNotSet) {
  char buf[37] = {};
  EXPECT_EQ(storage_get_uuid(buf), 0u);
  EXPECT_STREQ(buf, "");
}

TEST_F(StorageTest, SetUuidOverwritesPreviousValue) {
  storage_set_uuid("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  storage_set_uuid("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");

  char buf[37] = {};
  storage_get_uuid(buf);
  EXPECT_STREQ(buf, "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");
}

// ===========================================================================
// storage_set_features / storage_get_features
// ===========================================================================

TEST_F(StorageTest, GetFeaturesReturnsEmptyArrayWhenNotSet) {
  JsonDocument doc;
  JsonArray features = doc.to<JsonArray>();
  storage_get_features(features);
  EXPECT_EQ(features.size(), 0u);
}

TEST_F(StorageTest, SetAndGetFeatures) {
  JsonDocument doc;
  JsonArray features = doc.to<JsonArray>();
  JsonObject f = features.add<JsonObject>();
  f["type"] = "temperature";
  f["name"] = "Temperature";
  f["unit"] = "°C";

  size_t written = storage_set_features(features);
  EXPECT_GT(written, 0u);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_features(result);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_STREQ(result[0]["type"].as<const char*>(), "temperature");
  EXPECT_STREQ(result[0]["name"].as<const char*>(), "Temperature");
  EXPECT_STREQ(result[0]["unit"].as<const char*>(), "°C");
}

TEST_F(StorageTest, SetAndGetMultipleFeatures) {
  JsonDocument doc;
  JsonArray features = doc.to<JsonArray>();
  features.add<JsonObject>()["type"] = "temperature";
  features.add<JsonObject>()["type"] = "humidity";
  features.add<JsonObject>()["type"] = "light";

  storage_set_features(features);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_features(result);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_STREQ(result[0]["type"].as<const char*>(), "temperature");
  EXPECT_STREQ(result[1]["type"].as<const char*>(), "humidity");
  EXPECT_STREQ(result[2]["type"].as<const char*>(), "light");
}

TEST_F(StorageTest, SetFeaturesOverwritesPreviousValue) {
  // Overwrite with different data.
  JsonDocument doc1;
  JsonArray features1 = doc1.to<JsonArray>();
  features1.add<JsonObject>()["type"] = "temperature";
  storage_set_features(features1);

  JsonDocument doc2;
  JsonArray features2 = doc2.to<JsonArray>();
  features2.add<JsonObject>()["type"] = "humidity";
  storage_set_features(features2);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_features(result);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_STREQ(result[0]["type"].as<const char*>(), "humidity");

  // Overwrite with an empty array.
  JsonDocument empty_doc;
  storage_set_features(empty_doc.to<JsonArray>());

  JsonDocument empty_result_doc;
  JsonArray empty_result = empty_result_doc.to<JsonArray>();
  storage_get_features(empty_result);
  EXPECT_EQ(empty_result.size(), 0u);
}

// ===========================================================================
// UUID and features are stored independently
// ===========================================================================

TEST_F(StorageTest, UuidAndFeaturesAreIndependent) {
  // Writing UUID does not clear features.
  storage_set_uuid("cccccccc-cccc-cccc-cccc-cccccccccccc");

  JsonDocument doc;
  JsonArray features = doc.to<JsonArray>();
  features.add<JsonObject>()["type"] = "pressure";
  storage_set_features(features);

  char buf[37] = {};
  storage_get_uuid(buf);
  EXPECT_STREQ(buf, "cccccccc-cccc-cccc-cccc-cccccccccccc");

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_features(result);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_STREQ(result[0]["type"].as<const char*>(), "pressure");

  // Writing features does not clear UUID.
  storage_set_uuid("dddddddd-dddd-dddd-dddd-dddddddddddd");

  JsonDocument doc2;
  JsonArray features2 = doc2.to<JsonArray>();
  features2.add<JsonObject>()["type"] = "temperature";
  storage_set_features(features2);

  char buf2[37] = {};
  storage_get_uuid(buf2);
  EXPECT_STREQ(buf2, "dddddddd-dddd-dddd-dddd-dddddddddddd");

  JsonDocument result_doc2;
  JsonArray result2 = result_doc2.to<JsonArray>();
  storage_get_features(result2);
  ASSERT_EQ(result2.size(), 1u);
  EXPECT_STREQ(result2[0]["type"].as<const char*>(), "temperature");
}
