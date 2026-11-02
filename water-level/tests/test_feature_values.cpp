#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include <HomeAnthillCore.h>

class FeatureValuesTest : public ::testing::Test {
protected:
  void SetUp() override {
    feature_values_clear();
  }

  JsonArray makeFeatures(JsonDocument& doc) {
    JsonArray features = doc.to<JsonArray>();

    JsonObject humidity = features.add<JsonObject>();
    humidity["name"] = "humidity";
    humidity["unit"] = "%";

    JsonObject temperature = features.add<JsonObject>();
    temperature["name"] = "temperature";
    temperature["unit"] = "°C";

    JsonObject light = features.add<JsonObject>();
    light["name"] = "light";
    light["unit"] = "lux";

    return features;
  }
};

TEST_F(FeatureValuesTest, InitCreatesOneEntryPerFeatureWithoutValues) {
  JsonDocument doc;
  ha::feature_values_init(makeFeatures(doc), false);

  EXPECT_EQ(feature_values_count(), 3u);

  FeatureValue value = {};
  ASSERT_TRUE(feature_values_get(0, &value));
  EXPECT_STREQ(value.name, "humidity");
  EXPECT_STREQ(value.unit, "%");
  EXPECT_FALSE(value.has_value);

  ASSERT_TRUE(feature_values_get(1, &value));
  EXPECT_STREQ(value.name, "temperature");
  EXPECT_STREQ(value.unit, "°C");
  EXPECT_FALSE(value.has_value);

  ASSERT_TRUE(feature_values_get(2, &value));
  EXPECT_STREQ(value.name, "light");
  EXPECT_STREQ(value.unit, "lux");
  EXPECT_FALSE(value.has_value);
}

TEST_F(FeatureValuesTest, SetUpdatesExistingFeatureValue) {
  JsonDocument doc;
  ha::feature_values_init(makeFeatures(doc), false);

  EXPECT_TRUE(feature_values_set("temperature", 23.5f));

  FeatureValue value = {};
  ASSERT_TRUE(feature_values_get(1, &value));
  EXPECT_STREQ(value.name, "temperature");
  EXPECT_TRUE(value.has_value);
  EXPECT_FLOAT_EQ(value.value, 23.5f);
}

TEST_F(FeatureValuesTest, SetDoesNotCreateUnknownFeature) {
  JsonDocument doc;
  ha::feature_values_init(makeFeatures(doc), false);

  EXPECT_FALSE(feature_values_set("unknown", 1.0f));
  EXPECT_EQ(feature_values_count(), 3u);
}

TEST_F(FeatureValuesTest, GetRejectsInvalidInputs) {
  JsonDocument doc;
  ha::feature_values_init(makeFeatures(doc), false);

  FeatureValue value = {};
  EXPECT_FALSE(feature_values_get(3, &value));
  EXPECT_FALSE(feature_values_get(0, nullptr));
}

TEST_F(FeatureValuesTest, InitSkipsFeaturesWithoutNames) {
  JsonDocument doc;
  JsonArray features = doc.to<JsonArray>();

  JsonObject unnamed = features.add<JsonObject>();
  unnamed["unit"] = "%";

  JsonObject named = features.add<JsonObject>();
  named["name"] = "light";
  named["unit"] = "lux";

  ha::feature_values_init(features, false);

  EXPECT_EQ(feature_values_count(), 1u);
  FeatureValue value = {};
  ASSERT_TRUE(feature_values_get(0, &value));
  EXPECT_STREQ(value.name, "light");
}

TEST_F(FeatureValuesTest, InitSkipsOnlineFeature) {
  JsonDocument doc;
  JsonArray features = makeFeatures(doc);

  JsonObject online = features.add<JsonObject>();
  online["name"] = "online";
  online["unit"] = "-";

  ha::feature_values_init(features, false);

  EXPECT_EQ(feature_values_count(), 3u);
  EXPECT_FALSE(feature_values_set("online", 1.0f));
}
