#include <gtest/gtest.h>

// Mock headers first so production includes resolve to stubs.
#include "Wire.h"
#include "Air_Quality_Sensor.h"

#include "airquality_sensor.h"

// ---------------------------------------------------------------------------
// The production global current_air_value lives in airquality_sensor.cpp.
// Expose it here so SetUp() can reset it to -1 between tests, giving each
// test a clean slate without modifying production code.
// ---------------------------------------------------------------------------
extern int current_air_value;

// ---------------------------------------------------------------------------
// Fixture — resets mock state and production global before every test.
// ---------------------------------------------------------------------------

class AirqualitySensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    AirQualityMockState::reset();
    current_air_value = -1;
  }
};

// ===========================================================================
// airquality_get_value
// ===========================================================================

TEST_F(AirqualitySensorTest, GetValueReturnsMinusOneInitially) {
  EXPECT_EQ(airquality_get_value(), -1);
}

TEST_F(AirqualitySensorTest, GetValueReflectsValueAfterHasNewValue) {
  AirQualityMockState::instance().slope_result = AirQualitySensor::FRESH_AIR;
  airquality_has_newvalue();
  EXPECT_EQ(airquality_get_value(), AirQualitySensor::FRESH_AIR);
}

// ===========================================================================
// airquality_has_newvalue — quality-level transitions
// ===========================================================================

TEST_F(AirqualitySensorTest, HasNewValueReturnsTrueOnFirstValidReading) {
  // current_air_value starts at -1; any valid quality triggers a change.
  AirQualityMockState::instance().slope_result = AirQualitySensor::FRESH_AIR;
  EXPECT_TRUE(airquality_has_newvalue());
}

TEST_F(AirqualitySensorTest, HasNewValueReturnsFalseWhenValueUnchanged) {
  AirQualityMockState::instance().slope_result = AirQualitySensor::FRESH_AIR;
  airquality_has_newvalue();  // sets current_air_value = FRESH_AIR
  EXPECT_FALSE(airquality_has_newvalue());  // same value → no change
}

TEST_F(AirqualitySensorTest, HasNewValueReturnsTrueWhenLevelChanges) {
  AirQualityMockState::instance().slope_result = AirQualitySensor::FRESH_AIR;
  airquality_has_newvalue();

  AirQualityMockState::instance().slope_result = AirQualitySensor::HIGH_POLLUTION;
  EXPECT_TRUE(airquality_has_newvalue());
  EXPECT_EQ(airquality_get_value(), AirQualitySensor::HIGH_POLLUTION);
}

TEST_F(AirqualitySensorTest, HasNewValueReturnsFalseWhenNoRecognisedQuality) {
  // slope() returns a value that matches none of the four quality constants.
  AirQualityMockState::instance().slope_result = -99;
  EXPECT_FALSE(airquality_has_newvalue());
  EXPECT_EQ(airquality_get_value(), -1);  // current_air_value unchanged
}

// ===========================================================================
// airquality_has_newvalue — all quality levels are recognised
// ===========================================================================

TEST_F(AirqualitySensorTest, AllFourQualityLevelsAreRecognised) {
  const int levels[] = {
    AirQualitySensor::FORCE_SIGNAL,
    AirQualitySensor::HIGH_POLLUTION,
    AirQualitySensor::LOW_POLLUTION,
    AirQualitySensor::FRESH_AIR,
  };

  for (int level : levels) {
    current_air_value = -1;  // reset between sub-cases
    AirQualityMockState::instance().slope_result = level;
    EXPECT_TRUE(airquality_has_newvalue()) << "level=" << level;
    EXPECT_EQ(airquality_get_value(), level) << "level=" << level;
  }
}

// ===========================================================================
// airquality_has_newvalue — transitions between all consecutive levels
// ===========================================================================

TEST_F(AirqualitySensorTest, TransitionsBetweenAllLevelsAreDetected) {
  const int levels[] = {
    AirQualitySensor::FORCE_SIGNAL,
    AirQualitySensor::HIGH_POLLUTION,
    AirQualitySensor::LOW_POLLUTION,
    AirQualitySensor::FRESH_AIR,
  };

  AirQualityMockState::instance().slope_result = levels[0];
  airquality_has_newvalue();  // establish initial state

  for (size_t i = 1; i < sizeof(levels) / sizeof(levels[0]); ++i) {
    AirQualityMockState::instance().slope_result = levels[i];
    EXPECT_TRUE(airquality_has_newvalue()) << "transition to level=" << levels[i];
  }
}
