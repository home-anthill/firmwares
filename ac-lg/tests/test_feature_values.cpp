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
    online["type"] = "sensor";
    online["name"] = "online";
    online["unit"] = "-";

    JsonObject power = features.add<JsonObject>();
    power["type"] = "controller";
    power["name"] = "on";
    power["unit"] = "-";
    JsonObject powerSpec = power["spec"].to<JsonObject>();
    powerSpec["format"] = "bool";

    JsonObject temperature = features.add<JsonObject>();
    temperature["type"] = "sensor";
    temperature["name"] = "temperature";
    temperature["unit"] = "C";

    JsonObject setpoint = features.add<JsonObject>();
    setpoint["type"] = "controller";
    setpoint["name"] = "setpoint";
    setpoint["unit"] = "C";
    JsonObject setpointSpec = setpoint["spec"].to<JsonObject>();
    setpointSpec["format"] = "int";
    setpointSpec["min"] = 16;

    JsonObject mode = features.add<JsonObject>();
    mode["type"] = "controller";
    mode["name"] = "mode";
    mode["unit"] = "-";
    JsonObject modeSpec = mode["spec"].to<JsonObject>();
    modeSpec["format"] = "list";
    JsonArray modeList = modeSpec["list"].to<JsonArray>();
    JsonObject modeItem = modeList.add<JsonObject>();
    modeItem["value"] = 0;
    modeItem["text"] = "Cool";

    JsonObject fanSpeed = features.add<JsonObject>();
    fanSpeed["type"] = "controller";
    fanSpeed["name"] = "fanSpeed";
    fanSpeed["unit"] = "-";
    JsonObject fanSpeedSpec = fanSpeed["spec"].to<JsonObject>();
    fanSpeedSpec["format"] = "list";
    JsonArray fanSpeedList = fanSpeedSpec["list"].to<JsonArray>();
    JsonObject fanSpeedItem = fanSpeedList.add<JsonObject>();
    fanSpeedItem["value"] = 10;
    fanSpeedItem["text"] = "Max";

    return features;
  }
};

TEST_F(FeatureValuesTest, InitCreatesOneEntryPerFeatureWithoutValues) {
  JsonDocument doc;
  feature_values_init(makeFeatures(doc));

  EXPECT_EQ(feature_values_count(), 5u);

  FeatureValue value = {};
  ASSERT_TRUE(feature_values_get(0, &value));
  EXPECT_STREQ(value.name, "temperature");
  EXPECT_STREQ(value.unit, "C");
  EXPECT_FALSE(value.has_value);

  ASSERT_TRUE(feature_values_get(1, &value));
  EXPECT_STREQ(value.name, "on");
  EXPECT_TRUE(value.has_value);
  EXPECT_FLOAT_EQ(value.value, 0.0f);

  ASSERT_TRUE(feature_values_get(2, &value));
  EXPECT_STREQ(value.name, "setpoint");
  EXPECT_TRUE(value.has_value);
  EXPECT_FLOAT_EQ(value.value, 16.0f);

  ASSERT_TRUE(feature_values_get(3, &value));
  EXPECT_STREQ(value.name, "mode");
  EXPECT_TRUE(value.has_value);
  EXPECT_FLOAT_EQ(value.value, 0.0f);

  ASSERT_TRUE(feature_values_get(4, &value));
  EXPECT_STREQ(value.name, "fanSpeed");
  EXPECT_TRUE(value.has_value);
  EXPECT_FLOAT_EQ(value.value, 10.0f);

  EXPECT_FALSE(feature_values_set("online", 1.0f));
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
  EXPECT_EQ(feature_values_count(), 5u);
}

TEST_F(FeatureValuesTest, GetRejectsInvalidInputs) {
  JsonDocument doc;
  feature_values_init(makeFeatures(doc));

  FeatureValue value = {};
  EXPECT_FALSE(feature_values_get(5, &value));
  EXPECT_FALSE(feature_values_get(0, nullptr));
}
