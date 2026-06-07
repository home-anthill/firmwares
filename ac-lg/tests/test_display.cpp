#include <gtest/gtest.h>
#include <cstring>

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

  void addFeatureValue(size_t index, const char* name, const char* unit,
                       float value, bool has_value) {
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

TEST_F(DisplayTest, UpdateDisplayRendersFirstAvailableFeatureValue) {
  addFeatureValue(0, "online", "-", 1.0f, true);

  update_display();

  EXPECT_EQ(DisplayMockState::instance().clear_count, 1);
  EXPECT_EQ(DisplayMockState::instance().display_count, 1);
  ASSERT_GE(DisplayMockState::instance().prints.size(), 2u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "online");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "1.0 -");
}

TEST_F(DisplayTest, UpdateDisplayAdvancesThroughAvailableFeatureValues) {
  addFeatureValue(0, "online", "-", 1.0f, true);
  addFeatureValue(1, "setpoint", "°C", 22.0f, true);

  update_display();
  update_display();

  ASSERT_GE(DisplayMockState::instance().prints.size(), 4u);
  EXPECT_EQ(DisplayMockState::instance().prints[0], "online");
  EXPECT_EQ(DisplayMockState::instance().prints[1], "1.0 -");
  EXPECT_EQ(DisplayMockState::instance().prints[2], "setpoint");
  EXPECT_EQ(DisplayMockState::instance().prints[3], std::string("22.0 \xF8""C"));
}
