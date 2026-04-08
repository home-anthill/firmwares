#include <gtest/gtest.h>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

#include "display.h"

// display.cpp's file-scope flag — reset to true before every test so that
// a previous init failure does not bleed into the next test.
extern bool showDisplay;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class DisplayTest : public ::testing::Test {
protected:
  void SetUp() override {
    DisplayMockState::reset();
    showDisplay = true;  // restore default so each test starts fresh
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

// ===========================================================================
// update_display
// ===========================================================================

TEST_F(DisplayTest, UpdateDisplayRendersValue) {
  // Ensure the display is considered initialised.
  init_display();
  DisplayMockState::reset();  // zero counters after init

  update_display(22.5f);

  EXPECT_EQ(DisplayMockState::instance().clear_count,   1);
  EXPECT_EQ(DisplayMockState::instance().display_count, 1);
  EXPECT_FLOAT_EQ(DisplayMockState::instance().last_print_float, 22.5f);
}

TEST_F(DisplayTest, UpdateDisplayRendersNegativeValue) {
  init_display();
  DisplayMockState::reset();

  update_display(-3.0f);

  EXPECT_EQ(DisplayMockState::instance().display_count, 1);
  EXPECT_FLOAT_EQ(DisplayMockState::instance().last_print_float, -3.0f);
}

TEST_F(DisplayTest, UpdateDisplayDoesNothingWhenShowDisplayIsFalse) {
  showDisplay = false;

  update_display(25.0f);

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
  EXPECT_EQ(DisplayMockState::instance().clear_count,   0);
}

TEST_F(DisplayTest, UpdateDisplayAfterInitFailureDoesNothing) {
  DisplayMockState::instance().begin_result = false;
  init_display();  // sets showDisplay = false
  DisplayMockState::reset();

  update_display(25.0f);

  EXPECT_EQ(DisplayMockState::instance().display_count, 0);
}
