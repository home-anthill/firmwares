#include <gtest/gtest.h>
#include <cmath>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_I2CDevice.h"
#include "Adafruit_I2CRegister.h"
#include "Adafruit_MCP9600.h"

#include "temp_sensor.h"

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class TempSensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    TempSensorMockState::reset();
  }
};

// ===========================================================================
// temp_init_sensor
// ===========================================================================

TEST_F(TempSensorTest, InitSensorCallsBegin) {
  temp_init_sensor();
  EXPECT_TRUE(TempSensorMockState::instance().init_called);
}

TEST_F(TempSensorTest, InitSensorSucceedsWithDefaultMockState) {
  // begin_result defaults to true — init should not trigger ESP.restart().
  TempSensorMockState::instance().begin_result = true;
  temp_init_sensor();  // must not crash or hang
  EXPECT_TRUE(TempSensorMockState::instance().init_called);
}

// ===========================================================================
// temp_get_temperature
// ===========================================================================

TEST_F(TempSensorTest, GetTemperatureReturnsThermocoupleValue) {
  TempSensorMockState::instance().thermocouple_value = 23.5f;
  EXPECT_FLOAT_EQ(temp_get_temperature(), 23.5f);
}

TEST_F(TempSensorTest, GetTemperatureReturnsNegativeValue) {
  TempSensorMockState::instance().thermocouple_value = -5.0f;
  EXPECT_FLOAT_EQ(temp_get_temperature(), -5.0f);
}

TEST_F(TempSensorTest, GetTemperatureReturnsZero) {
  TempSensorMockState::instance().thermocouple_value = 0.0f;
  EXPECT_FLOAT_EQ(temp_get_temperature(), 0.0f);
}

TEST_F(TempSensorTest, GetTemperatureReturnsNanWhenMockReturnsNan) {
  TempSensorMockState::instance().thermocouple_value = NAN;
  float result = temp_get_temperature();
  EXPECT_TRUE(std::isnan(result));
}

TEST_F(TempSensorTest, GetTemperatureIncrementsReadCount) {
  temp_get_temperature();
  temp_get_temperature();
  EXPECT_EQ(TempSensorMockState::instance().read_count, 2);
}

TEST_F(TempSensorTest, GetTemperatureReturnsMockValueConsistently) {
  TempSensorMockState::instance().thermocouple_value = 18.75f;
  EXPECT_FLOAT_EQ(temp_get_temperature(), 18.75f);
  EXPECT_FLOAT_EQ(temp_get_temperature(), 18.75f);  // same value on repeated reads
}
