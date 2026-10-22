#include <gtest/gtest.h>
#include <Arduino.h>
#include "config.h"
#include "secrets.h"
#include "water_level_sensor.h"

TEST(WaterLevelSensorTest, ConfiguresDigitalInput) {
  GpioMockState::reset();
  water_level_init_sensor();
  EXPECT_EQ(GpioMockState::instance().pin_modes.at(WATER_LEVEL_PIN), INPUT);
}

TEST(WaterLevelSensorTest, ReadsBothStatesAndTransitionsWithConfiguredPolarity) {
  for (int state : {LOW, HIGH, LOW, HIGH}) {
    g_digital_read_value = state;
    EXPECT_EQ(water_level_read(), (state == HIGH) == WATER_LEVEL_ACTIVE_HIGH);
  }
}
