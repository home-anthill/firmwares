#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include "feature_values.h"

class FeatureValuesTest : public ::testing::Test {
protected:
  void SetUp() override {
    feature_values_clear();
  }

  JsonArray makeFeatures(JsonDocument& doc) {
    JsonArray features = doc.to<JsonArray>();

    JsonObject online = features.add<JsonObject>();
    online["name"] = "online";
    online["unit"] = "-";

    JsonObject power = features.add<JsonObject>();
    power["name"] = "on";
    power["unit"] = "-";

    JsonObject setpoint = features.add<JsonObject>();
    setpoint["name"] = "setpoint";
    setpoint["unit"] = "C";

    return features;
  }
};

TEST_F(FeatureValuesTest, InitCreatesOneEntryPerFeatureWithoutValues) {
  JsonDocument doc;
  feature_values_init(makeFeatures(doc));

  EXPECT_EQ(feature_values_count(), 3u);

  FeatureValue value = {};
  ASSERT_TRUE(feature_values_get(0, &value));
  EXPECT_STREQ(value.name, "online");
  EXPECT_STREQ(value.unit, "-");
  EXPECT_FALSE(value.has_value);

  ASSERT_TRUE(feature_values_get(1, &value));
  EXPECT_STREQ(value.name, "on");
  EXPECT_FALSE(value.has_value);
}

TEST_F(FeatureValuesTest, SetUpdatesExistingFeatureValue) {
  JsonDocument doc;
  feature_values_init(makeFeatures(doc));

  EXPECT_TRUE(feature_values_set("setpoint", 23.0f));

  FeatureValue value = {};
  ASSERT_TRUE(feature_values_get(2, &value));
  EXPECT_STREQ(value.name, "setpoint");
  EXPECT_TRUE(value.has_value);
  EXPECT_FLOAT_EQ(value.value, 23.0f);
}

TEST_F(FeatureValuesTest, SetDoesNotCreateUnknownFeature) {
  JsonDocument doc;
  feature_values_init(makeFeatures(doc));

  EXPECT_FALSE(feature_values_set("unknown", 1.0f));
  EXPECT_EQ(feature_values_count(), 3u);
}

TEST_F(FeatureValuesTest, GetRejectsInvalidInputs) {
  JsonDocument doc;
  feature_values_init(makeFeatures(doc));

  FeatureValue value = {};
  EXPECT_FALSE(feature_values_get(3, &value));
  EXPECT_FALSE(feature_values_get(0, nullptr));
}
