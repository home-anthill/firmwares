#include <gtest/gtest.h>
#include <cmath>
#include <SensirionI2cSen65.h>
#include "sen65_sensor.h"
#include "config.h"

class Sen65Test : public ::testing::Test {
  void SetUp() override {
    Wire = TwoWire{};
    sen65_mock = Sen65Mock{};
  }
};
TEST_F(Sen65Test, InitializesBusAndReadsAllFields) {
  sen65_init_sensor();
  EXPECT_EQ(Wire.sda, I2C_SDA);
  EXPECT_EQ(Wire.scl, I2C_SCL);
  EXPECT_EQ(Wire.clock, 100000);
  EXPECT_EQ(Wire.timeout, 100);
  EXPECT_EQ(sen65_mock.starts, 1);
  auto r = sen65_read();
  EXPECT_FLOAT_EQ(r.pm1, 1);
  EXPECT_FLOAT_EQ(r.pm25, 2);
  EXPECT_FLOAT_EQ(r.pm4, 3);
  EXPECT_FLOAT_EQ(r.pm10, 4);
  EXPECT_FLOAT_EQ(r.humidity, 50);
  EXPECT_FLOAT_EQ(r.temperature, 23);
  EXPECT_FLOAT_EQ(r.voc_index, 100);
  EXPECT_FLOAT_EQ(r.nox_index, 5);
}
TEST_F(Sen65Test, FailedReadDiscardsEveryField) {
  sen65_init_sensor();
  sen65_mock.read_error = 1;
  const auto r = sen65_read();
  EXPECT_TRUE(std::isnan(r.pm1));
  EXPECT_TRUE(std::isnan(r.pm25));
  EXPECT_TRUE(std::isnan(r.pm4));
  EXPECT_TRUE(std::isnan(r.pm10));
  EXPECT_TRUE(std::isnan(r.humidity));
  EXPECT_TRUE(std::isnan(r.temperature));
  EXPECT_TRUE(std::isnan(r.voc_index));
  EXPECT_TRUE(std::isnan(r.nox_index));
}
TEST_F(Sen65Test, InitializationFailuresPreventReadsIncludingAfterSuccessfulInit) {
  sen65_init_sensor();
  Wire.begin_ok = false;
  sen65_init_sensor();
  EXPECT_TRUE(std::isnan(sen65_read().temperature));
  Wire.begin_ok = true;
  Wire.bus_error = 2;
  sen65_init_sensor();
  EXPECT_TRUE(std::isnan(sen65_read().temperature));
  Wire.bus_error = 0;
  sen65_mock.reset_error = 1;
  sen65_init_sensor();
  EXPECT_TRUE(std::isnan(sen65_read().temperature));
  sen65_mock.reset_error = 0;
  sen65_mock.start_error = 1;
  sen65_init_sensor();
  EXPECT_TRUE(std::isnan(sen65_read().temperature));
  EXPECT_EQ(sen65_mock.reads, 0);
}
