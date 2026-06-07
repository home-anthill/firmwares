#include <gtest/gtest.h>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

#include <ArduinoJson.h>

#include "display.h"
#include "feature_values.h"

// display.cpp's file-scope flag — reset to true before every test so that
// a previous init failure does not bleed into the next test.
extern bool showDisplay;
extern size_t display_feature_index;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class DisplayTest : public ::testing::Test {
protected:
  void SetUp() override {
    DisplayMockState::reset();
    showDisplay = true;  // restore default so each test starts fresh
    display_feature_index = 0;
    feature_values_clear();
  }

  void initFeatureValues() {
    JsonDocument doc;
    JsonArray features = doc.to<JsonArray>();

    JsonObject setpoint = features.add<JsonObject>();
    setpoint["name"] = "setpoint";
    setpoint["unit"] = "°C";

    JsonObject temperature = features.add<JsonObject>();
    temperature["name"] = "temperature";
    temperature["unit"] = "°C";

    feature_values_init(features);
  }
};

// ===========================================================================
// init_display
// ===========================================================================

TEST_F(DisplayTest, InitDisplayCallsBeginOnce) {
  init_display();
  EXPECT_EQ(DisplayMockState::instance().begin_count, 1);
}

TEST_F(DisplayTest, InitDisplayClearsAndPrintsStarting) {
  init_display();
  // clearDisplay + print("Starting") + display() must all have been called.
  EXPECT_GE(DisplayMockState::instance().clear_count,   1);
  EXPECT_GE(DisplayMockState::instance().display_count, 1);
  EXPECT_EQ(DisplayMockState::instance().last_print_str, "Starting");
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

// ===========================================================================
// update_display
// ===========================================================================

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureValuesExist) {
  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenNoFeatureHasValueYet) {
  initFeatureValues();

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count, 0);
}

TEST_F(DisplayTest, UpdateDisplayRendersFirstAvailableFeatureValue) {
  initFeatureValues();
  ASSERT_TRUE(feature_values_set("temperature", 22.5f));
  init_display();
  DisplayMockState::reset();

  update_display();

  EXPECT_EQ(DisplayMockState::instance().clear_count, 1);
  EXPECT_EQ(DisplayMockState::instance().display_count, 1);
  EXPECT_EQ(DisplayMockState::instance().last_print_str, std::string("22.5 \xF8""C"));
}

TEST_F(DisplayTest, UpdateDisplayAdvancesThroughAvailableFeatureValues) {
  initFeatureValues();
  ASSERT_TRUE(feature_values_set("setpoint", 21.5f));
  ASSERT_TRUE(feature_values_set("temperature", 22.5f));
  init_display();
  DisplayMockState::reset();

  update_display();
  EXPECT_EQ(DisplayMockState::instance().last_print_str, std::string("21.5 \xF8""C"));

  update_display();
  EXPECT_EQ(DisplayMockState::instance().last_print_str, std::string("22.5 \xF8""C"));

  EXPECT_EQ(DisplayMockState::instance().display_count, 2);
}

TEST_F(DisplayTest, UpdateDisplaySkipsFeaturesWithoutValues) {
  initFeatureValues();
  ASSERT_TRUE(feature_values_set("temperature", 22.5f));
  init_display();
  DisplayMockState::reset();

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 1);
  EXPECT_EQ(DisplayMockState::instance().last_print_str, std::string("22.5 \xF8""C"));
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenShowDisplayIsFalse) {
  showDisplay = false;
  initFeatureValues();
  ASSERT_TRUE(feature_values_set("temperature", 25.0f));

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count,   0);
}

TEST_F(DisplayTest, UpdateDisplayAfterInitFailureDoesNothing) {
  DisplayMockState::instance().begin_result = false;
  init_display();  // sets showDisplay = false
  DisplayMockState::reset();
  initFeatureValues();
  ASSERT_TRUE(feature_values_set("temperature", 25.0f));

  update_display();

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
}
