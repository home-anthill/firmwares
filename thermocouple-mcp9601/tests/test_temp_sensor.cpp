#include <gtest/gtest.h>
#include <Arduino.h>
#include <Adafruit_MCP9601.h>
#include <cmath>
#include "temp_sensor.h"

class SensorTest : public ::testing::Test {
  void SetUp() override { TempSensorMockState::reset(); }
};
TEST_F(SensorTest, AcceptsMcp9601AndReadsTemperature) {
  temp_init_sensor();
  EXPECT_EQ(TempSensorMockState::instance().expected_device_id, 0x41);
  EXPECT_EQ(TempSensorMockState::instance().begin_address, 0x67);
  EXPECT_FLOAT_EQ(temp_get_temperature(), 22);
}
TEST_F(SensorTest, RejectsMcp9600) {
  TempSensorMockState::instance().device_id = 0x40;
  temp_init_sensor();
  EXPECT_TRUE(std::isnan(temp_get_temperature()));
}
TEST_F(SensorTest, RejectsProbeFaultsAndRecovers) {
  temp_init_sensor();
  for (auto status : {0x10, 0x20, 0x30}) {
    TempSensorMockState::instance().status = status;
    EXPECT_TRUE(std::isnan(temp_get_temperature()));
  }
  TempSensorMockState::instance().status = 0;
  EXPECT_FLOAT_EQ(temp_get_temperature(), 22);
}
TEST_F(SensorTest, RejectsInvalidAndOutOfRangeReadings) {
  temp_init_sensor();
  for (float value : {NAN, INFINITY, -201.0f, 1373.0f}) {
    TempSensorMockState::instance().thermocouple_value = value;
    EXPECT_TRUE(std::isnan(temp_get_temperature()));
  }
}
