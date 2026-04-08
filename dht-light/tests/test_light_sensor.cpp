#include <gtest/gtest.h>

// Mock headers first so production includes resolve to stubs.
#include "Wire.h"
#include "Digital_Light_TSL2561.h"

#include "light_sensor.h"

// ---------------------------------------------------------------------------
// Fixture — resets mock state before every test.
// ---------------------------------------------------------------------------

class LightSensorTest : public ::testing::Test {
protected:
  void SetUp() override { LightMockState::reset(); }
};

// ===========================================================================
// light_get_value
// ===========================================================================

TEST_F(LightSensorTest, GetValueReturnsLuxFromSensor) {
  LightMockState::instance().lux_value = 0;       // darkness / reset default
  EXPECT_EQ(light_get_value(), 0L);

  LightMockState::instance().lux_value = 1234;
  EXPECT_EQ(light_get_value(), 1234L);

  LightMockState::instance().lux_value = 100000;  // bright sunlight
  EXPECT_EQ(light_get_value(), 100000L);
}

TEST_F(LightSensorTest, GetValueReturnsNegativeOnSensorOverflow) {
  // TSL2561 returns -1 when the reading exceeds the chip's range.
  LightMockState::instance().lux_value = -1;
  EXPECT_EQ(light_get_value(), -1L);
}
