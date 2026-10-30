#include <gtest/gtest.h>
#include <cmath>
#include "SensirionI2cSht4x.h"
#include "config.h"
#include "secrets.h"
#include "temp_hum_sensor.h"

class Sht45SensorTest : public ::testing::Test {
protected:
  void SetUp() override {
    Sht45MockState::reset();
    Wire = TwoWire{};
  }
};

TEST_F(Sht45SensorTest, InitUsesConfiguredBusAndSht45Address) {
  sht45_init_sensor();
  EXPECT_EQ(Wire.sda, I2C_SDA);
  EXPECT_EQ(Wire.scl, I2C_SCL);
  EXPECT_EQ(Wire.begin_count, 1);
  EXPECT_EQ(Sht45MockState::instance().bus, &Wire);
  EXPECT_EQ(Sht45MockState::instance().address, 0x44);
  EXPECT_EQ(Sht45MockState::instance().reset_count, 1);
}

TEST_F(Sht45SensorTest, ReadsBothValuesInOneHighPrecisionMeasurement) {
  sht45_init_sensor();
  Sht45Reading reading;
  reading = sht45_read();
  EXPECT_FLOAT_EQ(reading.temperature, 23.5f);
  EXPECT_FLOAT_EQ(reading.humidity, 60.0f);
  EXPECT_EQ(Sht45MockState::instance().read_count, 1);
}

TEST_F(Sht45SensorTest, FailedReadInvalidatesBothPreviousValuesAndCanRecover) {
  sht45_init_sensor();
  Sht45Reading reading;
  reading = sht45_read();
  Sht45MockState::instance().read_error = 1;
  reading = sht45_read();
  EXPECT_TRUE(std::isnan(reading.temperature));
  EXPECT_TRUE(std::isnan(reading.humidity));
  Sht45MockState::instance().read_error = 0;
  Sht45MockState::instance().temperature = -12.5f;
  reading = sht45_read();
  EXPECT_FLOAT_EQ(reading.temperature, -12.5f);
  EXPECT_FLOAT_EQ(reading.humidity, 60.0f);
}

TEST_F(Sht45SensorTest, ResetFailureDoesNotPreventLaterRecovery) {
  Sht45MockState::instance().reset_error = 1;
  sht45_init_sensor();
  Sht45Reading reading;
  reading = sht45_read();
  EXPECT_FLOAT_EQ(reading.temperature, 23.5f);
}

TEST_F(Sht45SensorTest, RejectsNonfiniteMeasurementPairs) {
  sht45_init_sensor();
  for (float value : {NAN, INFINITY, -INFINITY}) {
    Sht45MockState::instance().temperature = value;
    Sht45Reading reading;
    reading = sht45_read();
    EXPECT_TRUE(std::isnan(reading.temperature));
    EXPECT_TRUE(std::isnan(reading.humidity));
    Sht45MockState::instance().temperature = 23.5f;
    Sht45MockState::instance().humidity = value;
    reading = sht45_read();
    EXPECT_TRUE(std::isnan(reading.temperature));
    EXPECT_TRUE(std::isnan(reading.humidity));
    Sht45MockState::instance().humidity = 60.0f;
  }
}

TEST_F(Sht45SensorTest, ClipsHumidityToPhysicalRange) {
  sht45_init_sensor();
  Sht45Reading reading;
  Sht45MockState::instance().humidity = -6.0f;
  reading = sht45_read();
  EXPECT_FLOAT_EQ(reading.humidity, 0.0f);
  Sht45MockState::instance().humidity = 119.0f;
  reading = sht45_read();
  EXPECT_FLOAT_EQ(reading.humidity, 100.0f);
}
