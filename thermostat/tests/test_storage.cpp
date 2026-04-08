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
  f["type"] = "controller";
  f["name"] = "setpoint";
  f["unit"] = "°C";

  size_t written = storage_set_features(features);
  EXPECT_GT(written, 0u);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_features(result);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_STREQ(result[0]["type"].as<const char*>(), "controller");
  EXPECT_STREQ(result[0]["name"].as<const char*>(), "setpoint");
  EXPECT_STREQ(result[0]["unit"].as<const char*>(), "°C");
}

TEST_F(StorageTest, SetAndGetMultipleFeatures) {
  JsonDocument doc;
  JsonArray features = doc.to<JsonArray>();
  features.add<JsonObject>()["name"] = "setpoint";
  features.add<JsonObject>()["name"] = "tolerance";
  features.add<JsonObject>()["name"] = "temperature";

  storage_set_features(features);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_features(result);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_STREQ(result[0]["name"].as<const char*>(), "setpoint");
  EXPECT_STREQ(result[1]["name"].as<const char*>(), "tolerance");
  EXPECT_STREQ(result[2]["name"].as<const char*>(), "temperature");
}

TEST_F(StorageTest, SetFeaturesOverwritesPreviousValue) {
  JsonDocument doc1;
  JsonArray features1 = doc1.to<JsonArray>();
  features1.add<JsonObject>()["name"] = "setpoint";
  storage_set_features(features1);

  JsonDocument doc2;
  JsonArray features2 = doc2.to<JsonArray>();
  features2.add<JsonObject>()["name"] = "tolerance";
  storage_set_features(features2);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_features(result);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_STREQ(result[0]["name"].as<const char*>(), "tolerance");

  // Overwrite with an empty array.
  JsonDocument empty_doc;
  storage_set_features(empty_doc.to<JsonArray>());

  JsonDocument empty_result_doc;
  JsonArray empty_result = empty_result_doc.to<JsonArray>();
  storage_get_features(empty_result);
  EXPECT_EQ(empty_result.size(), 0u);
}

// ===========================================================================
// storage_set_feature_values / storage_get_feature_values  (thermostat-only)
// ===========================================================================

TEST_F(StorageTest, GetFeatureValuesReturnsEmptyArrayWhenNotSet) {
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  storage_get_feature_values(featureValues);
  EXPECT_EQ(featureValues.size(), 0u);
}

TEST_F(StorageTest, SetAndGetFeatureValues) {
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  JsonObject fv = featureValues.add<JsonObject>();
  fv["featureName"] = "setpoint";
  fv["value"]       = 22.0f;

  size_t written = storage_set_feature_values(featureValues);
  EXPECT_GT(written, 0u);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_feature_values(result);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_STREQ(result[0]["featureName"].as<const char*>(), "setpoint");
  EXPECT_FLOAT_EQ(result[0]["value"].as<float>(), 22.0f);
}

TEST_F(StorageTest, SetAndGetMultipleFeatureValues) {
  JsonDocument doc;
  JsonArray featureValues = doc.to<JsonArray>();
  JsonObject sp = featureValues.add<JsonObject>();
  sp["featureName"] = "setpoint";
  sp["value"]       = 20.0f;
  JsonObject tol = featureValues.add<JsonObject>();
  tol["featureName"] = "tolerance";
  tol["value"]       = 3.0f;

  storage_set_feature_values(featureValues);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_feature_values(result);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_STREQ(result[0]["featureName"].as<const char*>(), "setpoint");
  EXPECT_FLOAT_EQ(result[0]["value"].as<float>(), 20.0f);
  EXPECT_STREQ(result[1]["featureName"].as<const char*>(), "tolerance");
  EXPECT_FLOAT_EQ(result[1]["value"].as<float>(), 3.0f);
}

TEST_F(StorageTest, SetFeatureValuesOverwritesPreviousValue) {
  JsonDocument doc1;
  JsonArray fv1 = doc1.to<JsonArray>();
  fv1.add<JsonObject>()["featureName"] = "setpoint";
  storage_set_feature_values(fv1);

  JsonDocument doc2;
  JsonArray fv2 = doc2.to<JsonArray>();
  fv2.add<JsonObject>()["featureName"] = "tolerance";
  storage_set_feature_values(fv2);

  JsonDocument result_doc;
  JsonArray result = result_doc.to<JsonArray>();
  storage_get_feature_values(result);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_STREQ(result[0]["featureName"].as<const char*>(), "tolerance");
}

// ===========================================================================
// UUID, features, and feature values are stored independently
// ===========================================================================

TEST_F(StorageTest, AllThreeKeysAreIndependent) {
  // Write all three independent keys.
  storage_set_uuid("cccccccc-cccc-cccc-cccc-cccccccccccc");

  JsonDocument fdoc;
  JsonArray features = fdoc.to<JsonArray>();
  features.add<JsonObject>()["name"] = "temperature";
  storage_set_features(features);

  JsonDocument vdoc;
  JsonArray featureValues = vdoc.to<JsonArray>();
  featureValues.add<JsonObject>()["featureName"] = "setpoint";
  storage_set_feature_values(featureValues);

  // Verify each key independently.
  char buf[37] = {};
  storage_get_uuid(buf);
  EXPECT_STREQ(buf, "cccccccc-cccc-cccc-cccc-cccccccccccc");

  JsonDocument fr;
  JsonArray fresult = fr.to<JsonArray>();
  storage_get_features(fresult);
  ASSERT_EQ(fresult.size(), 1u);
  EXPECT_STREQ(fresult[0]["name"].as<const char*>(), "temperature");

  JsonDocument vr;
  JsonArray vresult = vr.to<JsonArray>();
  storage_get_feature_values(vresult);
  ASSERT_EQ(vresult.size(), 1u);
  EXPECT_STREQ(vresult[0]["featureName"].as<const char*>(), "setpoint");
}
