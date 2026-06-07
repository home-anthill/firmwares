#include <gtest/gtest.h>
#include <cstring>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

#include "display.h"
#include "feature_values.h"

static FeatureValue g_feature_values[3];
static size_t g_feature_values_count = 0;

void feature_values_init(JsonArray /*features*/) {}
void feature_values_clear() {}

size_t feature_values_count() {
  return g_feature_values_count;
}

bool feature_values_set(const char* /*name*/, float /*value*/) {
  return false;
}

bool feature_values_get(size_t index, FeatureValue* value) {
  if (value == nullptr || index >= g_feature_values_count) {
    return false;
  }

  *value = g_feature_values[index];
  return true;
}

// display.cpp's file-scope flag. Reset it before every test so that a previous
// init failure does not bleed into the next test.
extern bool showDisplay;
extern size_t display_feature_index;

class DisplayTest : public ::testing::Test {
protected:
  void SetUp() override {
    DisplayMockState::reset();
    showDisplay = true;
    display_feature_index = 0;
    memset(g_feature_values, 0, sizeof(g_feature_values));
    g_feature_values_count = 0;
  }

  void addFeatureValue(size_t index, const char* name, const char* unit, float value, bool has_value) {
    ASSERT_LT(index, 3u);
    strncpy(g_feature_values[index].name, name, sizeof(g_feature_values[index].name) - 1);
    strncpy(g_feature_values[index].unit, unit, sizeof(g_feature_values[index].unit) - 1);
    g_feature_values[index].value = value;
    g_feature_values[index].has_value = has_value;
    if (g_feature_values_count <= index) {
      g_feature_values_count = index + 1;
    }
  }
};

TEST_F(DisplayTest, InitDisplayCallsBeginOnce) {
  init_display();

  EXPECT_EQ(DisplayMockState::instance().begin_count, 1);
}

TEST_F(DisplayTest, InitDisplayClearsAndPrintsStarting) {
  init_display();

  EXPECT_GE(DisplayMockState::instance().clear_count, 1);
  EXPECT_GE(DisplayMockState::instance().display_count, 1);
  EXPECT_STREQ(DisplayMockState::instance().last_print_str, "Starting");
}

TEST_F(DisplayTest, InitDisplayLeavesShowDisplayTrueOnSuccess) {
  DisplayMockState::instance().begin_result = true;

  init_display();

  EXPECT_TRUE(showDisplay);
}

TEST_F(DisplayTest, InitDisplaySetsShowDisplayFalseOnBeginFailure) {
  DisplayMockState::instance().begin_result = false;

  init_display();

  EXPECT_FALSE(showDisplay);
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureValuesExist) {
  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureHasValueYet) {
  addFeatureValue(0, "temperature", "°C", 0.0f, false);
  addFeatureValue(1, "humidity", "%", 0.0f, false);

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, UpdateDisplayRendersFirstAvailableFeatureValue) {
  addFeatureValue(0, "temperature", "°C", 23.5f, true);

  update_display();

  EXPECT_EQ(DisplayMockState::instance().clear_count, 1);
  EXPECT_EQ(DisplayMockState::instance().display_count, 1);
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.5 \xF8""C"));
}

TEST_F(DisplayTest, UpdateDisplaySkipsFeaturesWithoutValues) {
  addFeatureValue(0, "temperature", "°C", 0.0f, false);
  addFeatureValue(1, "humidity", "%", 60.0f, true);

  update_display();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "humidity");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "60.0 %");
}

TEST_F(DisplayTest, UpdateDisplayAdvancesThroughAvailableFeatureValues) {
  addFeatureValue(0, "temperature", "°C", 23.5f, true);
  addFeatureValue(1, "humidity", "%", 60.0f, true);

  update_display();
  update_display();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 4u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "temperature");
  EXPECT_EQ(DisplayMockState::instance().prints[1], std::string("23.5 \xF8""C"));
  EXPECT_EQ(DisplayMockState::instance().prints[2], "humidity");
  EXPECT_EQ(DisplayMockState::instance().prints[3], "60.0 %");
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenDisplayIsDisabled) {
  showDisplay = false;
  addFeatureValue(0, "temperature", "°C", 23.5f, true);

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}
