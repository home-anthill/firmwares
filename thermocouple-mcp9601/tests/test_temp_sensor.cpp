#include <gtest/gtest.h>
#include <Arduino.h>
#include <Adafruit_MCP9601.h>
#include <cmath>
#include "temp_sensor.h"
#include "config.h"

constexpr uint8_t selected_device_id = 0x41;

class SensorTest : public ::testing::Test {
  void SetUp() override {
    TempSensorMockState::reset();
    TempSensorMockState::instance().device_id = selected_device_id;
  }
};
TEST_F(SensorTest, AcceptsMcp9601AndReadsTemperature) {
  temp_init_sensor();
  EXPECT_EQ(TempSensorMockState::instance().expected_device_id, selected_device_id);
  EXPECT_EQ(TempSensorMockState::instance().begin_address, THERMOCOUPLE_I2C_ADDRESS);
  EXPECT_EQ(TempSensorMockState::instance().thermocouple_type, THERMOCOUPLE_DRIVER_TYPE);
  const auto reading = temp_get_temperature();
  EXPECT_FLOAT_EQ(reading.temperature, TempSensorMockState::instance().thermocouple_value);
  EXPECT_EQ(reading.error, TEMP_SENSOR_OK);
}
TEST_F(SensorTest, RejectsMcp9600) {
  TempSensorMockState::instance().device_id = 0x40;
  temp_init_sensor();
  const auto reading = temp_get_temperature();
  EXPECT_TRUE(std::isnan(reading.temperature));
  EXPECT_EQ(reading.error, TEMP_SENSOR_INIT_ERROR);
}
TEST_F(SensorTest, RejectsProbeFaultsAndRecovers) {
  temp_init_sensor();
  for (auto status : {0x10, 0x20, 0x30}) {
    TempSensorMockState::instance().status = status;
    const auto reading = temp_get_temperature();
    EXPECT_TRUE(std::isnan(reading.temperature));
    EXPECT_EQ(reading.error, status == 0x10 ? TEMP_SENSOR_OPEN_CIRCUIT :
                              (status == 0x20 ? TEMP_SENSOR_SHORT_CIRCUIT : TEMP_SENSOR_OPEN_AND_SHORT));
  }
  TempSensorMockState::instance().status = 0;
  const auto reading = temp_get_temperature();
  EXPECT_FLOAT_EQ(reading.temperature, TempSensorMockState::instance().thermocouple_value);
  EXPECT_EQ(reading.error, TEMP_SENSOR_OK);
}
TEST_F(SensorTest, RejectsInvalidAndOutOfRangeReadings) {
  temp_init_sensor();
  for (float value : {NAN, INFINITY, -201.0f, 1373.0f}) {
    TempSensorMockState::instance().thermocouple_value = value;
    const auto reading = temp_get_temperature();
    EXPECT_TRUE(std::isnan(reading.temperature));
    EXPECT_EQ(reading.error, TEMP_SENSOR_INVALID_READING);
  }
}
