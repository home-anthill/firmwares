#include <gtest/gtest.h>
#include <cmath>

// Mock headers first so production includes resolve to stubs.
#include "DHT_U.h"

#include "dht_sensor.h"

// ---------------------------------------------------------------------------
// Fixture — resets mock state before every test.
// ---------------------------------------------------------------------------

class DhtSensorTest : public ::testing::Test {
protected:
  void SetUp() override { DhtMockState::reset(); }
};

// ===========================================================================
// dht_get_temperature
// ===========================================================================

TEST_F(DhtSensorTest, GetTemperatureReturnsValueFromSensor) {
  DhtMockState::instance().temp_value = 23.5f;
  EXPECT_FLOAT_EQ(dht_get_temperature(), 23.5f);

  DhtMockState::instance().temp_value = -10.0f;
  EXPECT_FLOAT_EQ(dht_get_temperature(), -10.0f);

  DhtMockState::instance().temp_value = -40.0f;  // sensor min
  EXPECT_FLOAT_EQ(dht_get_temperature(), -40.0f);

  DhtMockState::instance().temp_value = 80.0f;   // sensor max
  EXPECT_FLOAT_EQ(dht_get_temperature(), 80.0f);
}

TEST_F(DhtSensorTest, GetTemperatureReturnsNanOnReadFailure) {
  DhtMockState::instance().temp_event_result = false;
  EXPECT_TRUE(std::isnan(dht_get_temperature()));
}

// ===========================================================================
// dht_get_humidity
// ===========================================================================

TEST_F(DhtSensorTest, GetHumidityReturnsValueFromSensor) {
  DhtMockState::instance().hum_value = 60.0f;
  EXPECT_FLOAT_EQ(dht_get_humidity(), 60.0f);

  DhtMockState::instance().hum_value = 0.0f;    // sensor min
  EXPECT_FLOAT_EQ(dht_get_humidity(), 0.0f);

  DhtMockState::instance().hum_value = 100.0f;  // sensor max
  EXPECT_FLOAT_EQ(dht_get_humidity(), 100.0f);
}

TEST_F(DhtSensorTest, GetHumidityReturnsNanOnReadFailure) {
  DhtMockState::instance().hum_event_result = false;
  EXPECT_TRUE(std::isnan(dht_get_humidity()));
}

// ===========================================================================
// Independence — temperature and humidity read failures are independent
// ===========================================================================

TEST_F(DhtSensorTest, SensorReadingsAreIndependent) {
  // Temperature failure must not affect humidity.
  DhtMockState::instance().temp_event_result = false;
  DhtMockState::instance().hum_value         = 55.0f;
  EXPECT_TRUE(std::isnan(dht_get_temperature()));
  EXPECT_FLOAT_EQ(dht_get_humidity(), 55.0f);

  // Humidity failure must not affect temperature.
  DhtMockState::reset();
  DhtMockState::instance().hum_event_result = false;
  DhtMockState::instance().temp_value       = 21.0f;
  EXPECT_FLOAT_EQ(dht_get_temperature(), 21.0f);
  EXPECT_TRUE(std::isnan(dht_get_humidity()));
}
