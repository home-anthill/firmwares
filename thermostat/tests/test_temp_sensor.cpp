#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Mock headers first so production includes resolve to stubs.
#include <Arduino.h>
#include "Wire.h"
#include "Adafruit_I2CDevice.h"
#include "Adafruit_I2CRegister.h"
#include "Adafruit_MCP9600.h"

#include "secrets.h"
#include "temp_sensor.h"

#ifndef THERMOCOUPLE_TYPE
#define THERMOCOUPLE_TYPE "K"
#endif

static MCP9600_ThermocoupleType expectedThermocoupleType() {
  if (strcmp(THERMOCOUPLE_TYPE, "K") == 0) return MCP9600_TYPE_K;
  if (strcmp(THERMOCOUPLE_TYPE, "J") == 0) return MCP9600_TYPE_J;
  if (strcmp(THERMOCOUPLE_TYPE, "T") == 0) return MCP9600_TYPE_T;
  if (strcmp(THERMOCOUPLE_TYPE, "N") == 0) return MCP9600_TYPE_N;
  if (strcmp(THERMOCOUPLE_TYPE, "S") == 0) return MCP9600_TYPE_S;
  if (strcmp(THERMOCOUPLE_TYPE, "E") == 0) return MCP9600_TYPE_E;
  if (strcmp(THERMOCOUPLE_TYPE, "B") == 0) return MCP9600_TYPE_B;
  if (strcmp(THERMOCOUPLE_TYPE, "R") == 0) return MCP9600_TYPE_R;
  return MCP9600_TYPE_K;
}

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

TEST_F(TempSensorTest, InitSensorUsesConfiguredThermocoupleType) {
  temp_init_sensor();

  EXPECT_EQ(TempSensorMockState::instance().set_thermocouple_type_count, 1);
  EXPECT_EQ(TempSensorMockState::instance().thermocouple_type,
            expectedThermocoupleType());
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
