#include <gtest/gtest.h>
#include <cmath>

// Mock headers first so production includes resolve to stubs.
#include "Wire.h"
#include "Dps3xx.h"

#include "barometer_sensor.h"

// ---------------------------------------------------------------------------
// Fixture — resets mock state before every test.
// ---------------------------------------------------------------------------

class BarometerSensorTest : public ::testing::Test {
protected:
  void SetUp() override { Dps3xxMockState::reset(); }
};

// ===========================================================================
// barometer_get_temperature
// ===========================================================================

TEST_F(BarometerSensorTest, GetTemperatureReturnsValueFromSensor) {
  Dps3xxMockState::instance().temp_value   = 21.5f;
  Dps3xxMockState::instance().temp_success = true;
  EXPECT_FLOAT_EQ(barometer_get_temperature(), 21.5f);

  Dps3xxMockState::instance().temp_value = -10.0f;
  EXPECT_FLOAT_EQ(barometer_get_temperature(), -10.0f);

  Dps3xxMockState::instance().temp_value = 0.0f;
  EXPECT_FLOAT_EQ(barometer_get_temperature(), 0.0f);

  Dps3xxMockState::instance().temp_value = 85.0f;  // sensor upper range
  EXPECT_FLOAT_EQ(barometer_get_temperature(), 85.0f);
}

TEST_F(BarometerSensorTest, GetTemperatureReturnsNanOnReadFailure) {
  Dps3xxMockState::instance().temp_success = false;
  EXPECT_TRUE(std::isnan(barometer_get_temperature()));
}

// ===========================================================================
// barometer_get_airpressure
// ===========================================================================

TEST_F(BarometerSensorTest, GetAirpressureReturnsPressureDividedByThousand) {
  // Production code divides raw Pa reading by 1000.
  Dps3xxMockState::instance().pressure_value   = 101325.0f;
  Dps3xxMockState::instance().pressure_success = true;
  EXPECT_FLOAT_EQ(barometer_get_airpressure(), 101325.0f / 1000.0f);
}

TEST_F(BarometerSensorTest, GetAirpressureReturnsNanOnReadFailure) {
  Dps3xxMockState::instance().pressure_success = false;
  EXPECT_TRUE(std::isnan(barometer_get_airpressure()));
}

TEST_F(BarometerSensorTest, GetAirpressureHandlesVariousRawValues) {
  Dps3xxMockState::instance().pressure_success = true;

  Dps3xxMockState::instance().pressure_value = 0.0f;
  EXPECT_FLOAT_EQ(barometer_get_airpressure(), 0.0f);

  Dps3xxMockState::instance().pressure_value = 50000.0f;
  EXPECT_FLOAT_EQ(barometer_get_airpressure(), 50.0f);
}

// ===========================================================================
// Independence — temperature and airpressure read failures are independent
// ===========================================================================

TEST_F(BarometerSensorTest, SensorReadingsAreIndependent) {
  // Temperature failure must not affect airpressure.
  Dps3xxMockState::instance().temp_success     = false;
  Dps3xxMockState::instance().pressure_value   = 101325.0f;
  Dps3xxMockState::instance().pressure_success = true;
  EXPECT_TRUE(std::isnan(barometer_get_temperature()));
  EXPECT_FLOAT_EQ(barometer_get_airpressure(), 101325.0f / 1000.0f);

  // Airpressure failure must not affect temperature.
  Dps3xxMockState::reset();
  Dps3xxMockState::instance().temp_value       = 22.0f;
  Dps3xxMockState::instance().temp_success     = true;
  Dps3xxMockState::instance().pressure_success = false;
  EXPECT_FLOAT_EQ(barometer_get_temperature(), 22.0f);
  EXPECT_TRUE(std::isnan(barometer_get_airpressure()));
}
